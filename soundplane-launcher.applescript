-- Launches MEC for the Soundplane in a Terminal window.
-- Stop it with Ctrl-C ONCE (graceful USB shutdown) -- never force-quit / close the window.
-- Must live in the repo root: the repo is located relative to this script.
set repoDir to POSIX path of ((path to me as text) & "::")
tell application "Terminal"
	activate
	do script "cd " & quoted form of repoDir & " && clear && echo '>>> MEC Soundplane running. Press Ctrl-C ONCE to stop cleanly. Do NOT close this window or force-quit (USB kernel-panic risk).' && exec ./build/release/bin/mec-app mec-soundplane.json"
end tell
