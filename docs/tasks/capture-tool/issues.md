# Issues: capture-tool

## ISSUE-001: The edit's backup is armed before it holds the file
Status: open
Severity: minor
Source: review
Found in cycle: 1
Reopened: 0

### Problem
`tools/captures.sh`, "Temporary edit": `edit_backup="$(mktemp ...)"` is assigned before `cp "$root/$edit_file" "$edit_backup"` runs. The EXIT trap restores from `edit_backup` whenever it is non-empty, so an interrupt between the `mktemp` and the end of the `cp` makes `cleanup` copy an empty or partial backup over the source file, which is the opposite of "restored byte for byte ... on success, failure or interrupt" (Decisions; acceptance criterion 3). Arm the restore only once the backup is complete (e.g. copy into a local temp path, then set `edit_backup` to it; the scratch backup file may be left behind if interrupted before arming, which is harmless). Keep the edit itself after the arming, as now.

### Fix notes
