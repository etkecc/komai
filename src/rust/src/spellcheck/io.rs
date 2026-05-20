// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Filesystem helpers: dictionary search directories, the user-home
//! lookup, and the personal-dictionary word-list file format.

use super::*;

pub(super) fn dictionary_search_dirs(data_dir: &Path) -> Vec<PathBuf> {
    let mut dirs: Vec<PathBuf> = Vec::new();
    let mut push = |p: PathBuf| {
        if !p.as_os_str().is_empty() && !dirs.contains(&p) {
            dirs.push(p);
        }
    };

    // Per-user override dir: `<data_dir>/hunspell/` — `data_dir` is the
    // profile/app data directory C++ hands us.
    if !data_dir.as_os_str().is_empty() {
        push(data_dir.join("hunspell"));
    }

    if cfg!(target_os = "macos") {
        if let Some(home) = dirs_home() {
            push(home.join("Library/Spelling"));
        }
        push(PathBuf::from("/Library/Spelling"));
    }

    if cfg!(unix) && !cfg!(target_os = "macos") {
        // XDG data dirs (covers Flatpak's Hunspell.* extensions mounted under
        // /usr/share/hunspell as well).
        let xdg_data_home = std::env::var_os("XDG_DATA_HOME")
            .map(PathBuf::from)
            .or_else(|| dirs_home().map(|h| h.join(".local/share")));
        if let Some(h) = xdg_data_home {
            push(h.join("hunspell"));
        }
        let xdg_data_dirs = std::env::var_os("XDG_DATA_DIRS")
            .map(|v| v.to_string_lossy().into_owned())
            .unwrap_or_else(|| "/usr/local/share:/usr/share".to_owned());
        for d in xdg_data_dirs.split(':').filter(|s| !s.is_empty()) {
            push(PathBuf::from(d).join("hunspell"));
        }
        push(PathBuf::from("/usr/share/hunspell"));
        push(PathBuf::from("/usr/share/myspell"));
        push(PathBuf::from("/usr/share/myspell/dicts"));
    }

    dirs
}

pub(super) fn dirs_home() -> Option<PathBuf> {
    std::env::var_os("HOME").map(PathBuf::from).filter(|p| !p.as_os_str().is_empty())
}

// ---------------------------------------------------------------------------
// Tokenisation / skip ranges
// ---------------------------------------------------------------------------

/// Byte ranges in `text` that should not be word-checked: URLs and emails,
/// inline `` `code` `` spans, `@mentions` / `#tags`, and `:emoji_shortcode:`
/// runs. (Triple-backtick fenced blocks are handled a block at a time by the

pub(super) fn read_word_list_file(path: &Path) -> HashSet<String> {
    let Ok(contents) = std::fs::read_to_string(path) else {
        return HashSet::new();
    };
    contents
        .lines()
        .map(|l| l.trim())
        .filter(|l| !l.is_empty() && !l.starts_with('#'))
        .map(|l| l.to_lowercase())
        .collect()
}

pub(super) fn append_word_to_file(path: &Path, word: &str) {
    use std::io::Write;
    if let Some(parent) = path.parent() {
        let _ = std::fs::create_dir_all(parent);
    }
    if let Ok(mut f) = std::fs::OpenOptions::new().create(true).append(true).open(path) {
        let _ = writeln!(f, "{word}");
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
