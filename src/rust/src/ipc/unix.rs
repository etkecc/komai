// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::collections::BTreeSet;
use std::env;
use std::io;
use std::os::unix::net::UnixStream;
use std::path::PathBuf;

fn socket_name(profile: &str) -> String {
    let normalized = if profile.is_empty() || profile == "default" {
        "default"
    } else {
        profile
    };

    format!("komai-cli-{normalized}")
}

fn socket_paths(profile: &str) -> Vec<PathBuf> {
    let socket_name = socket_name(profile);
    let mut dirs = Vec::new();

    if let Some(runtime_dir) = env::var_os("XDG_RUNTIME_DIR") {
        dirs.push(PathBuf::from(runtime_dir));
    }
    if let Some(tmp_dir) = env::var_os("TMPDIR") {
        dirs.push(PathBuf::from(tmp_dir));
    }
    dirs.push(env::temp_dir());
    dirs.push(PathBuf::from("/tmp"));

    let mut unique = BTreeSet::new();
    for dir in dirs {
        unique.insert(dir.join(&socket_name));
    }

    unique.into_iter().collect()
}

pub fn connect(profile: &str) -> Result<UnixStream, String> {
    let normalized_profile = if profile.is_empty() || profile == "default" {
        "default"
    } else {
        profile
    };

    let mut last_error: Option<io::Error> = None;
    for path in socket_paths(normalized_profile) {
        match UnixStream::connect(&path) {
            Ok(stream) => return Ok(stream),
            Err(error) => last_error = Some(error),
        }
    }

    let mut message =
        format!("no running Komai instance for profile '{normalized_profile}'. Start Komai first: komai");
    if normalized_profile != "default" {
        message.push_str(&format!(" -p {normalized_profile}"));
    }

    if let Some(error) = last_error {
        match error.kind() {
            io::ErrorKind::NotFound | io::ErrorKind::ConnectionRefused => {}
            _ => message.push_str(&format!(" ({error})")),
        }
    }

    Err(message)
}

#[cfg(test)]
mod tests {
    use super::socket_paths;

    #[test]
    fn default_profile_socket_name_is_normalized() {
        let paths = socket_paths("");
        assert!(paths.iter().all(|path| {
            path.file_name()
                .and_then(|name| name.to_str())
                == Some("komai-cli-default")
        }));
    }
}
