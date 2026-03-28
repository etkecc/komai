// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::io;

const PIPE_PREFIX: &str = "\\\\.\\pipe\\";
#[cfg(windows)]
const PIPE_CONNECT_TIMEOUT_MS: u32 = 5000;
const ERROR_FILE_NOT_FOUND: i32 = 2;
const ERROR_PATH_NOT_FOUND: i32 = 3;
#[cfg(windows)]
const ERROR_PIPE_BUSY: i32 = 231;

fn pipe_path(profile: &str) -> String {
    format!("{PIPE_PREFIX}{}", super::socket_name(profile))
}

fn connection_error_message(profile: &str, error: &io::Error) -> String {
    let normalized_profile = super::normalized_profile(profile);

    let mut message =
        format!("no running Komai instance for profile '{normalized_profile}'. Start Komai first: komai");
    if normalized_profile != "default" {
        message.push_str(&format!(" -p {normalized_profile}"));
    }

    match error.raw_os_error() {
        Some(ERROR_FILE_NOT_FOUND | ERROR_PATH_NOT_FOUND) => {}
        _ => message.push_str(&format!(" ({error})")),
    }

    message
}

#[cfg(windows)]
fn open_pipe(path: &str) -> io::Result<std::fs::File> {
    use std::fs::OpenOptions;

    loop {
        match OpenOptions::new().read(true).write(true).open(path) {
            Ok(file) => return Ok(file),
            Err(error) if error.raw_os_error() == Some(ERROR_PIPE_BUSY) => {
                wait_for_pipe(path)?;
            }
            Err(error) => return Err(error),
        }
    }
}

#[cfg(windows)]
fn wait_for_pipe(path: &str) -> io::Result<()> {
    use std::ffi::OsStr;
    use std::os::windows::ffi::OsStrExt;

    use windows_sys::Win32::System::Pipes::WaitNamedPipeW;

    let mut wide_path: Vec<u16> = OsStr::new(path).encode_wide().collect();
    wide_path.push(0);

    let waited = unsafe { WaitNamedPipeW(wide_path.as_ptr(), PIPE_CONNECT_TIMEOUT_MS) };
    if waited == 0 {
        return Err(io::Error::last_os_error());
    }

    Ok(())
}

#[cfg(windows)]
pub fn connect(profile: &str) -> Result<std::fs::File, String> {
    let path = pipe_path(profile);
    open_pipe(&path).map_err(|error| connection_error_message(profile, &error))
}

#[cfg(test)]
mod tests {
    use std::io;

    use super::{connection_error_message, pipe_path};

    #[test]
    fn default_profile_pipe_name_is_normalized() {
        assert_eq!(pipe_path(""), r"\\.\pipe\komai-cli-default");
    }

    #[test]
    fn non_default_profile_pipe_name_is_preserved() {
        assert_eq!(pipe_path("work"), r"\\.\pipe\komai-cli-work");
    }

    #[test]
    fn missing_pipe_keeps_friendly_error_message() {
        let error = io::Error::from_raw_os_error(2);
        assert_eq!(
            connection_error_message("work", &error),
            "no running Komai instance for profile 'work'. Start Komai first: komai -p work"
        );
    }

    #[test]
    fn non_missing_pipe_error_is_appended() {
        let error = io::Error::other("pipe busy");
        assert_eq!(
            connection_error_message("default", &error),
            "no running Komai instance for profile 'default'. Start Komai first: komai (pipe busy)"
        );
    }
}
