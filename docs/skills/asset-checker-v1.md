// 09 03 2026, 15 41
/* purpose
* check that changed assets are stored, tracked, loaded, and owned correctly
* protect gameplay assets from accidental ignore rules or wrong runtime paths
* distinguish tracked game content from local production materials
* this skill DOES NOT commit music production files
* this skill DOES NOT replace visual or audio human acceptance
* this skill DOES NOT download or expose unapproved external content
*/

# Asset Checker v1

Check the asset path, file type, Git status, runtime loader, configuration, and
fallback behavior. Confirm gameplay sound belongs under the tracked entity, UI,
or weapon folders. Confirm music production files remain excluded. Report
missing files, wrong paths, ignore-rule conflicts, and the exact focused check
needed to prove the asset loads.
