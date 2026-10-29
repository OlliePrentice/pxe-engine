## game-developer cycle 1
- doc-gap: the spec's CLAUDE.md replacement text includes the "Keyboard, mouse and gamepad..." sentence, which in the file is a separate following paragraph together with the "Give the user a playtest checklist" sentence. I merged them into one paragraph rather than duplicating the sentence.
- doc-gap: macOS `sed -i` needs an empty suffix argument (`sed -i ''`); used for the temporary negative-check edits. zsh `$PIPESTATUS` is lowercase `$pipestatus`, which matters when checking exit status through a pipe.
- tool: none
