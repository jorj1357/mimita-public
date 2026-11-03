import unittest

from deploy.vps_storage_config import (
    remove_coturn_verbose,
    remove_duplicate_ubuntu_mirror_entries,
    remove_redundant_backup_cron_lines,
    update_rsyslog_rotation,
)


class StorageConfigTests(unittest.TestCase):
    def test_coturn_removes_only_the_exact_verbose_directive(self):
        source = "realm=turn.example\nverbose\n# verbose for future use\nstatic-auth-secret=private\n"
        changed, did_change = remove_coturn_verbose(source)
        self.assertTrue(did_change)
        self.assertEqual(changed, "realm=turn.example\n# verbose for future use\nstatic-auth-secret=private\n")

    def test_coturn_rejects_ambiguous_duplicate_directive(self):
        with self.assertRaisesRegex(ValueError, "multiple_standalone_verbose"):
            remove_coturn_verbose("verbose\nverbose\n")

    def test_apt_removes_only_exactly_duplicated_mirror_lines(self):
        source = (
            "# keep this comment\n"
            "deb [arch=amd64] http://archive.ubuntu.com/ubuntu jammy main restricted\n"
            "deb [arch=amd64] http://ubuntu.mirror.constant.com/ubuntu jammy main restricted\n"
            "deb https://deb.nodesource.com/node_22.x nodistro main\n"
        )
        changed, count = remove_duplicate_ubuntu_mirror_entries(source)
        self.assertEqual(count, 1)
        self.assertIn("# keep this comment", changed)
        self.assertIn("archive.ubuntu.com", changed)
        self.assertIn("deb.nodesource.com", changed)
        self.assertNotIn("ubuntu.mirror.constant.com", changed)

    def test_apt_preserves_mirror_when_definitions_differ(self):
        source = (
            "deb http://archive.ubuntu.com/ubuntu jammy main\n"
            "deb http://ubuntu.mirror.constant.com/ubuntu jammy universe\n"
        )
        with self.assertRaisesRegex(ValueError, "not_exact_duplicates"):
            remove_duplicate_ubuntu_mirror_entries(source)

    def test_logrotate_changes_only_the_syslog_rule(self):
        source = (
            "/var/log/syslog\n/var/log/messages\n{\n"
            "\trotate 4\n\tweekly\n\tmissingok\n\tcompress\n\tdelaycompress\n"
            "\tpostrotate\n\t\tservice rsyslog rotate\n\tendscript\n}\n"
            "/var/log/other.log {\n\tdaily\n\trotate 2\n}\n"
        )
        changed, did_change = update_rsyslog_rotation(source)
        self.assertTrue(did_change)
        self.assertIn("\tdaily\n\trotate 7\n\tmaxsize 100M\n", changed)
        self.assertIn("\tmissingok\n", changed)
        self.assertIn("\tpostrotate\n\t\tservice rsyslog rotate\n", changed)
        self.assertIn("/var/log/other.log {\n\tdaily\n\trotate 2\n}", changed)

    def test_logrotate_requires_one_syslog_rule(self):
        with self.assertRaisesRegex(ValueError, "expected_one_syslog"):
            update_rsyslog_rotation("/var/log/other.log {\n}\n")

    def test_legacy_backup_cron_removal_preserves_snapshots_and_other_jobs(self):
        source = (
            "# Daily full backup\n"
            "0 3 * * * sudo -u postgres pg_dumpall | gzip > /root/db_backups/backup-$(date +\\%F).sql.gz && rclone copy /root/db_backups gdrive:ravgrowth-db-backup-v4\n"
            "5 3 * * * find /root/db_backups -type f -mtime +30 -delete\n"
            "15 3 * * * zip -r /root/vps-snapshot/ravgrowth-$(date +\\%F).zip /root/ravgrowth-all && rclone sync /root/vps-snapshot gdrive:vps-snapshots\n"
            "0 3 * * * /root/scripts/dump-all-dbs.sh\n"
            "* * * * * echo test >> /root/cron_test.log\n"
        )
        changed, removed = remove_redundant_backup_cron_lines(source)
        self.assertEqual(removed, frozenset({"old_direct_dump", "duplicate_dump_script", "old_backup_prune"}))
        self.assertIn("/root/vps-snapshot/ravgrowth-", changed)
        self.assertIn("/root/scripts/dump-all-dbs.sh", source)
        self.assertNotIn("/root/scripts/dump-all-dbs.sh", changed)
        self.assertIn("/root/cron_test.log", changed)

    def test_legacy_backup_cron_keeps_nonmatching_or_comment_lines(self):
        source = (
            "# pg_dumpall is only a note\n"
            "0 3 * * * /root/scripts/backup_ravbotdb.sh\n"
            "15 3 * * * rclone sync /root/vps-snapshot gdrive:vps-snapshots\n"
        )
        changed, removed = remove_redundant_backup_cron_lines(source)
        self.assertEqual(changed, source)
        self.assertEqual(removed, frozenset())


if __name__ == "__main__":
    unittest.main()
