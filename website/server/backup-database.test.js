// 09 06 2026, 16 30
/* purpose
* Exercise backup failures and publication using offline fake PostgreSQL tools.
* Verify credential handling, required entries, and preservation of prior files.
* Keep all synthetic files in uniquely allocated temporary directories.
* DOES NOT connect to a database or load the real website environment.
* DOES NOT demonstrate PostgreSQL restore validity.
* DOES NOT create or delete user backups.
*/
import { test } from 'node:test'
import assert from 'node:assert/strict'
import { spawnSync } from 'node:child_process'
import { chmodSync, mkdtempSync, readFileSync, readdirSync, statSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { backupDatabase, backupEnvironment } from './backup-database.mjs'

const tables = ['users', 'game_stats', 'processed_events', 'vip_join_tickets']
const toc = tables.flatMap((name, index) => [
    `${index + 1}; 1259 123 TABLE public ${name} owner`,
    `${index + 10}; 0 123 TABLE DATA public ${name} owner`
]).join('\n')
const secret = 'fake-password-never-print'
const env = { PATH: process.env.PATH, PGPASSWORD: secret, PGDATABASE: 'synthetic' }
function fixture() {
    const path = mkdtempSync(join(tmpdir(), 'mimita-backup-test-'))
    chmodSync(path, 0o700)
    writeFileSync(join(path, 'existing-user.dump'), 'preserve exactly')
    return path
}
function fakeRun({ dumpStatus = 0, header = 'PGDMPfake-archive', listStatus = 0, list = toc, stderr = '' } = {}) {
    return (command, args, options) => {
        assert.equal(options.shell, false)
        assert.ok(!args.join(' ').includes(secret))
        if (command === 'pg_dump') {
            assert.equal(options.env.PGPASSWORD, secret)
            assert.deepEqual(args, ['--format=custom', '--no-password', '--lock-wait-timeout=10000'])
            writeFileSync(options.stdio[1], header)
            return { status: dumpStatus, stderr }
        }
        assert.equal(command, 'pg_restore')
        assert.equal(args[0], '--list')
        assert.equal(options.env.PGPASSWORD, undefined)
        return { status: listStatus, stdout: list }
    }
}
test('valid fake archive publishes uniquely, preserves previous files, and never claims restore proof', () => {
    const outputDirectory = fixture()
    for (let i = 0; i < 2; ++i) {
        const result = backupDatabase({ outputDirectory, env, run: fakeRun() })
        assert.equal(result.success, true)
        assert.equal(result.restoreValidated, false)
        assert.equal(readFileSync(join(outputDirectory, result.archive), 'utf8'), 'PGDMPfake-archive')
        if (process.platform !== 'win32') assert.equal(statSync(join(outputDirectory, result.archive)).mode & 0o777, 0o600)
    }
    assert.equal(readdirSync(outputDirectory).length, 3)
    assert.equal(readFileSync(join(outputDirectory, 'existing-user.dump'), 'utf8'), 'preserve exactly')
})
for (const [name, options, category] of [
    ['nonzero dump with apparently valid output', { dumpStatus: 1 }, 'pg_dump'],
    ['empty dump', { header: '' }, 'archive_header'],
    ['plain SQL', { header: 'CREATE TABLE users ();' }, 'archive_header'],
    ['failed archive list', { listStatus: 1 }, 'archive_list'],
    ['comment-only list', { list: '; TABLE public users owner' }, 'archive_structure'],
    ['partial progression ledger', { list: toc + '\n40; 1259 123 TABLE public progression_sessions owner' }, 'archive_structure'],
    ['missing data section', { list: toc.replace(/.*TABLE DATA public game_stats.*\n/, '') }, 'archive_structure'],
    ['raw warning contains credentials', { stderr: secret }, 'pg_dump']
]) {
    test(`${name}: no success archive and old backup untouched`, () => {
        const outputDirectory = fixture()
        const result = backupDatabase({ outputDirectory, env, run: fakeRun(options) })
        assert.equal(result.success, false)
        assert.equal(result.category, `backup_${category}_failed`)
        assert.equal(JSON.stringify(result).includes(secret), false)
        assert.deepEqual(readdirSync(outputDirectory).filter(name => name.endsWith('.dump')), ['existing-user.dump'])
        assert.equal(readFileSync(join(outputDirectory, 'existing-user.dump'), 'utf8'), 'preserve exactly')
    })
}
test('missing executable or timeout fails without publishing', () => {
    for (const code of ['ENOENT', 'ETIMEDOUT']) {
        const outputDirectory = fixture()
        const result = backupDatabase({ outputDirectory, env, run: () => ({ error: { code }, status: null }) })
        assert.equal(result.category, 'backup_pg_dump_failed')
        assert.deepEqual(readdirSync(outputDirectory).filter(name => name.endsWith('.dump')), ['existing-user.dump'])
    }
})
test('actual spawned fake pg_dump exits nonzero: no success archive', () => {
    const outputDirectory = fixture()
    const run = (_command, _args, options) => spawnSync(process.execPath,
        ['-e', 'process.stdout.write("PGDMPfake"); process.stderr.write("private-error"); process.exit(23)'], options)
    const result = backupDatabase({ outputDirectory, env, run })
    assert.equal(result.success, false)
    assert.equal(result.category, 'backup_pg_dump_failed')
    assert.deepEqual(readdirSync(outputDirectory).filter(name => name.endsWith('.dump')), ['existing-user.dump'])
})
test('dotenv parses quoted secrets and URL is converted solely to environment fields', () => {
    const path = join(fixture(), '.env')
    writeFileSync(path, 'DB_HOST=localhost\nDB_PORT=5432\nDB_USER=test\nDB_NAME=synthetic\nDB_PASSWORD="abc # with space"\nUNRELATED_SECRET=never\n')
    let result = backupEnvironment(path, {})
    assert.equal(result.PGPASSWORD, 'abc # with space')
    assert.equal(result.UNRELATED_SECRET, undefined)
    result = backupEnvironment(path, { DATABASE_URL: 'postgresql://test:a%23%20b@localhost:5433/synthetic?sslmode=require' })
    assert.equal(result.PGPASSWORD, 'a# b')
    assert.equal(result.PGPORT, '5433')
    assert.equal(result.PGSSLMODE, 'require')
    assert.equal(result.DATABASE_URL, undefined)
    assert.throws(() => backupEnvironment(path, { DATABASE_URL: 'postgresql://test:a@localhost/db?options=unsafe' }))
    assert.throws(() => backupEnvironment(path, { DB_NAME: 'host=wrong' }))
})
test('unsafe directory is rejected before running tools', { skip: process.platform === 'win32' }, () => {
    const outputDirectory = fixture()
    chmodSync(outputDirectory, 0o755)
    const result = backupDatabase({ outputDirectory, env, run: () => assert.fail('must not run') })
    assert.equal(result.category, 'backup_output_directory_failed')
})
test('CLI help is read-only and invalid invocation has nonzero exit without raw error', () => {
    const script = fileURLToPath(new URL('./backup-database.mjs', import.meta.url))
    const help = spawnSync(process.execPath, [script, '--help'], { encoding: 'utf8' })
    assert.equal(help.status, 0)
    const invalid = spawnSync(process.execPath, [script], { encoding: 'utf8' })
    assert.equal(invalid.status, 1)
    assert.equal(JSON.parse(invalid.stderr).category, 'backup_arguments_failed')
})
