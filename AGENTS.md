# Workspace Rules

## Deletion safety

- Never permanently delete a file or directory as the first action.
- Before removing anything, move the exact target into the operating system's Trash so it remains recoverable.
- Resolve and verify the exact target path before moving it. Never use a broad path, unresolved variable, or wildcard as a deletion target.
- On macOS, use a recoverable Trash operation (for example Finder Trash or moving the verified item to `~/.Trash`) instead of `rm`.
- If moving the target to Trash is unavailable, unsafe, or requires permission that has not been granted, stop and ask the user. Do not fall back to permanent deletion.
- After moving material items to Trash, report what was moved and that it can be recovered from Trash.
