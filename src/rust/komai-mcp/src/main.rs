// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::env;
use std::process;

use komai_mcp::mcp::server::{serve_stdio, ServerConfig};
use komai_mcp::mcp::tools::AccessMode;

fn main() {
    if let Err(error) = run() {
        eprintln!("Error: {error}");
        process::exit(1);
    }
}

fn run() -> Result<(), String> {
    match parse_args(env::args().skip(1))? {
        Command::Serve(config) => serve_stdio(config),
        Command::Help => {
            print_help();
            Ok(())
        }
    }
}

enum Command {
    Help,
    Serve(ServerConfig),
}

fn parse_args(args: impl IntoIterator<Item = String>) -> Result<Command, String> {
    let mut args = args.into_iter();
    let Some(command) = args.next() else {
        return Ok(Command::Help);
    };

    if command == "--help" || command == "-h" {
        return Ok(Command::Help);
    }

    if command != "serve" {
        return Err(format!(
            "Unknown subcommand '{command}'. Only 'serve' is supported."
        ));
    }

    let mut profile = String::from("default");
    let mut access_mode = AccessMode::ReadOnly;
    let mut pending_flag: Option<String> = None;

    for arg in args {
        if let Some(flag) = pending_flag.take() {
            match flag.as_str() {
                "-p" | "--profile" => {
                    profile = normalize_and_validate_profile(&arg)?;
                }
                "--access" => {
                    access_mode = AccessMode::parse(&arg).ok_or_else(|| {
                        "Invalid --access value. Use 'read_only' or 'read_write'.".to_owned()
                    })?;
                }
                _ => unreachable!("unknown pending flag"),
            }
            continue;
        }

        match arg.as_str() {
            "--help" | "-h" => return Ok(Command::Help),
            "-p" | "--profile" | "--access" => pending_flag = Some(arg),
            _ if arg.starts_with("--profile=") => {
                profile = normalize_and_validate_profile(
                    arg.trim_start_matches("--profile="),
                )?;
            }
            _ if arg.starts_with("--access=") => {
                access_mode = AccessMode::parse(arg.trim_start_matches("--access=")).ok_or_else(
                    || "Invalid --access value. Use 'read_only' or 'read_write'.".to_owned(),
                )?;
            }
            _ => {
                return Err(format!("Unexpected argument '{arg}'."));
            }
        }
    }

    if let Some(flag) = pending_flag {
        return Err(format!("Missing value for {flag}."));
    }

    Ok(Command::Serve(ServerConfig {
        profile,
        access_mode,
    }))
}

fn normalize_and_validate_profile(profile: &str) -> Result<String, String> {
    let normalized = if profile.is_empty() || profile == "default" {
        "default".to_owned()
    } else {
        profile.to_owned()
    };

    validate_profile(&normalized)?;
    Ok(normalized)
}

fn validate_profile(profile: &str) -> Result<(), String> {
    if profile.is_empty() {
        return Ok(());
    }
    if profile.len() > 64 {
        return Err("profile id must be at most 64 characters".to_owned());
    }
    if profile.starts_with('.') || profile.ends_with('.') {
        return Err("profile id must not start or end with '.'".to_owned());
    }

    for ch in profile.chars() {
        if ch.is_ascii_alphanumeric() || matches!(ch, '.' | '_' | '-') {
            continue;
        }

        if ch.is_ascii_control() {
            return Err("profile id must not contain control characters".to_owned());
        }
        if !ch.is_ascii() {
            return Err(
                "profile id must contain ASCII characters only (A-Z, a-z, 0-9, '.', '_', '-')"
                    .to_owned(),
            );
        }
        return Err("profile id may contain only A-Z, a-z, 0-9, '.', '_', '-'".to_owned());
    }

    Ok(())
}

fn print_help() {
    println!(
        "Usage: komai-mcp serve [--profile <profile>] [--access read_only|read_write]\n\n\
         Exposes the running Komai profile as an MCP server over stdio.\n\
         The target Komai profile must already be running.\n\n\
         Options:\n\
           -p, --profile <profile>   Target profile (default: default)\n\
               --access <mode>       Access mode: read_only or read_write (default: read_only)\n"
    );
}

#[cfg(test)]
mod tests {
    use super::{parse_args, Command};
    use komai_mcp::mcp::tools::AccessMode;

    #[test]
    fn parse_serve_defaults_to_default_profile_and_read_only() {
        let command = parse_args(["serve".to_owned()]).unwrap();
        match command {
            Command::Serve(config) => {
                assert_eq!(config.profile, "default");
                assert_eq!(config.access_mode, AccessMode::ReadOnly);
            }
            Command::Help => panic!("expected serve command"),
        }
    }

    #[test]
    fn parse_serve_supports_equals_style_flags() {
        let command = parse_args([
            "serve".to_owned(),
            "--profile=work".to_owned(),
            "--access=read_write".to_owned(),
        ])
        .unwrap();

        match command {
            Command::Serve(config) => {
                assert_eq!(config.profile, "work");
                assert_eq!(config.access_mode, AccessMode::ReadWrite);
            }
            Command::Help => panic!("expected serve command"),
        }
    }
}
