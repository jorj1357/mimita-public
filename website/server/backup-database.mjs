// 09 06 2026, 16 30
/* purpose
* Export the website database into a unique PostgreSQL custom archive.
* Publish only after dump exit, magic, and archive structure checks pass.
* Emit fixed operational status categories without raw tool output.
* DOES NOT restore databases, run migrations, or schedule backups.
* DOES NOT overwrite or remove existing backups.
* DOES NOT claim that archive inspection proves a successful restore.
*/
import dotenv from 'dotenv'
import { spawnSync } from 'node:child_process'
import { randomUUID } from 'node:crypto'
import { closeSync, fsyncSync, fstatSync, linkSync, lstatSync, mkdtempSync,
    openSync, readFileSync, readSync, rmdirSync, unlinkSync } from 'node:fs'
import { dirname, join, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const scriptPath = fileURLToPath(import.meta.url)
const defaultEnvFile = resolve(dirname(scriptPath), '../.env')
const requiredTables = ['users', 'game_stats', 'processed_events', 'vip_join_tickets']

// Separate configuration from execution so tests never load production settings.
export function backupEnvironment(envFile = defaultEnvFile, inherited = process.env) {
    const config = { ...dotenv.parse(readFileSync(envFile)), ...inherited }
    let host = config.DB_HOST, port = config.DB_PORT || '5432'
    let user = config.DB_USER, password = config.DB_PASSWORD, database = config.DB_NAME
    let sslmode = config.DB_SSL === 'true' ? 'require' : 'prefer'
    if (config.DATABASE_URL) {
        const url = new URL(config.DATABASE_URL)
        if (!['postgres:', 'postgresql:'].includes(url.protocol)
            || [...url.searchParams.keys()].some(key => key !== 'sslmode') || url.hash) {
            throw new Error('unsupported_database_url')
        }
        host = url.hostname.replace(/^\[|\]$/g, '')
        port = url.port || '5432'
        user = decodeURIComponent(url.username)
        password = decodeURIComponent(url.password)
        database = decodeURIComponent(url.pathname.slice(1))
        sslmode = url.searchParams.get('sslmode') || sslmode
    }
    if (![host, user, password, database].every(value => typeof value === 'string' && value.length > 0 && !value.includes('\0'))
        || !/^\d+$/.test(port) || Number(port) < 1 || Number(port) > 65535
        || !['disable', 'allow', 'prefer', 'require', 'verify-ca', 'verify-full'].includes(sslmode)
        // libpq treats a database value containing '=' or a URL as connection info.
        || database.includes('=') || /^postgres(?:ql)?:/i.test(database)) {
        throw new Error('invalid_database_configuration')
    }
    return {
        PATH: inherited.PATH || inherited.Path || process.env.PATH,
        ...(process.platform === 'win32' ? { SystemRoot: process.env.SystemRoot } : {}),
        LC_ALL: 'C', LANG: 'C',
        PGHOST: host, PGPORT: String(port), PGUSER: user, PGPASSWORD: password,
        PGDATABASE: database, PGSSLMODE: sslmode, PGCONNECT_TIMEOUT: '10',
        PGOPTIONS: '-c default_transaction_read_only=on'
    }
}

// No existing backup owner was found. run is injectable solely for offline tests.
export function backupDatabase({ outputDirectory, env, run = spawnSync }) {
    let stage = 'output_directory', file, partialDirectory, archiveName
    try {
        const output = resolve(outputDirectory)
        const directory = lstatSync(output)
        if (!directory.isDirectory() || directory.isSymbolicLink()
            || (process.platform !== 'win32' && ((directory.mode & 0o077) !== 0 || directory.uid !== process.getuid()))) {
            throw new Error('private_directory_required')
        }
        stage = 'archive_create'
        archiveName = `mimita-${new Date().toISOString().replace(/[:.]/g, '-')}-${randomUUID()}.dump`
        partialDirectory = mkdtempSync(join(output, '.incomplete-'))
        const partial = join(partialDirectory, 'archive.partial')
        file = openSync(partial, 'wx+', 0o600)
        stage = 'pg_dump'
        const dump = run('pg_dump', ['--format=custom', '--no-password', '--lock-wait-timeout=10000'], {
            env, shell: false, windowsHide: true, timeout: 900000,
            stdio: ['ignore', file, 'pipe'], maxBuffer: 1024 * 1024
        })
        if (dump.error || dump.status !== 0 || dump.signal) throw new Error('dump_failed')
        // Fail closed on warnings too; never publish an unexplained partial dump.
        if (dump.stderr?.length) throw new Error('dump_diagnostics')
        stage = 'archive_header'
        const bytes = fstatSync(file).size
        const magic = Buffer.alloc(5)
        if (bytes <= 5 || readSync(file, magic, 0, 5, 0) !== 5 || magic.toString('ascii') !== 'PGDMP') {
            throw new Error('invalid_archive')
        }
        fsyncSync(file)
        closeSync(file)
        file = undefined
        stage = 'archive_list'
        const list = run('pg_restore', ['--list', partial], {
            // Archive inspection has no need for database credentials.
            env: { PATH: env.PATH, LC_ALL: 'C', LANG: 'C', ...(env.SystemRoot ? { SystemRoot: env.SystemRoot } : {}) },
            shell: false, windowsHide: true, timeout: 60000,
            stdio: ['ignore', 'pipe', 'pipe'], encoding: 'utf8', maxBuffer: 16 * 1024 * 1024
        })
        if (list.error || list.status !== 0 || list.signal || list.stderr?.length) throw new Error('list_failed')
        stage = 'archive_structure'
        const entries = String(list.stdout).split(/\r?\n/).filter(line => /^\d+;\s+\d+\s+\d+\s/.test(line))
        const progressionTables = ['schema_migrations', 'progression_sessions', 'progression_players']
        const hasProgression = entries.some(line => / TABLE public progression_(sessions|players)\s/.test(line))
        for (const table of [...requiredTables, ...(hasProgression ? progressionTables : [])]) {
            for (const type of ['TABLE', 'TABLE DATA']) {
                if (!entries.some(line => new RegExp(`^\\d+;\\s+\\d+\\s+\\d+\\s+${type} public ${table}\\s+`).test(line))) {
                    throw new Error('required_table_missing')
                }
            }
        }
        stage = 'archive_publish'
        // link is atomic and fails on EEXIST; rename could overwrite a user backup.
        linkSync(partial, join(output, archiveName))
        if (process.platform !== 'win32') {
            const directoryFd = openSync(output, 'r')
            try { fsyncSync(directoryFd) } finally { closeSync(directoryFd) }
        }
        // These two paths were exclusively created by this invocation.
        stage = 'staging_cleanup'
        unlinkSync(partial)
        rmdirSync(partialDirectory)
        return { success: true, category: 'backup_archive_verified', archive: archiveName, bytes,
            entries: entries.length, restoreValidated: false }
    } catch {
        return { success: false, category: `backup_${stage}_failed`, restoreValidated: false }
    } finally {
        if (file !== undefined) closeSync(file)
    }
}

if (process.argv[1] && resolve(process.argv[1]) === scriptPath) {
    const args = process.argv.slice(2)
    if (args.length === 1 && args[0] === '--help') {
        console.log('Usage: node website/server/backup-database.mjs --output-dir /absolute/private/directory\nReads website/.env. POSIX only; directory must already exist, be owned by this user, and have mode 0700. Does not restore or schedule.')
    } else {
        let result
        if (args.length !== 2 || args[0] !== '--output-dir' || !args[1].startsWith('/')) {
            result = { success: false, category: 'backup_arguments_failed' }
        } else if (process.platform === 'win32') {
            result = { success: false, category: 'backup_posix_permissions_required' }
        } else {
            try { result = backupDatabase({ outputDirectory: args[1], env: backupEnvironment() }) }
            catch { result = { success: false, category: 'backup_configuration_failed' } }
        }
        console[result.success ? 'log' : 'error'](JSON.stringify({ ...result, at: new Date().toISOString() }))
        process.exitCode = result.success ? 0 : 1
    }
}
