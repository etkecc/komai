# fish completion for komai (auto-generated; do not edit)

function __komai_positionals
    set -l value_flags --access --alias-localpart --before-event-id --caption --content-type --creation-content --fetch-mode --filename --format --initial-state --invite --limit --log-level --log-type --msgtype --name --power-levels --preset --profile --reason --room-version --topic --variant -L -l -p
    set -l tokens (commandline -opc)
    set -l skip_next 0
    set -l first 1
    for token in $tokens
        if test $first -eq 1
            set first 0
            continue
        end
        if test $skip_next -eq 1
            set skip_next 0
            continue
        end
        if contains -- $token $value_flags
            set skip_next 1
            continue
        end
        if string match -q -- '-*' $token
            continue
        end
        echo $token
    end
end

function __komai_at_path
    # Exact positional match: used for subcommand-name completion at a
    # given level. Fires only when the user has typed exactly `$argv`.
    set -l got (__komai_positionals)
    set -l want $argv
    if test (count $got) -ne (count $want)
        return 1
    end
    for i in (seq (count $want))
        if test "$got[$i]" != "$want[$i]"
            return 1
        end
    end
    return 0
end

function __komai_within
    # Prefix match: used for flag completion inside a leaf subcommand.
    # Fires whenever the positional path starts with `$argv` (inclusive).
    set -l got (__komai_positionals)
    set -l want $argv
    if test (count $got) -lt (count $want)
        return 1
    end
    for i in (seq (count $want))
        if test "$got[$i]" != "$want[$i]"
            return 1
        end
    end
    return 0
end

complete -c komai -f

complete -c komai -n '__komai_at_path' -l debug
complete -c komai -n '__komai_at_path' -l help
complete -c komai -n '__komai_at_path' -l log-level
complete -c komai -n '__komai_at_path' -l log-type
complete -c komai -n '__komai_at_path' -l profile
complete -c komai -n '__komai_at_path' -l version
complete -c komai -n '__komai_at_path' -s L
complete -c komai -n '__komai_at_path' -s h
complete -c komai -n '__komai_at_path' -s l
complete -c komai -n '__komai_at_path' -s p
complete -c komai -n '__komai_at_path' -s v
complete -c komai -n '__komai_at_path' -a 'app'
complete -c komai -n '__komai_at_path' -a 'completions'
complete -c komai -n '__komai_at_path' -a 'mcp'
complete -c komai -n '__komai_at_path' -a 'media'
complete -c komai -n '__komai_at_path' -a 'profiles'
complete -c komai -n '__komai_at_path' -a 'rooms'
complete -c komai -n '__komai_at_path' -a 'settings'
complete -c komai -n '__komai_at_path' -a 'theme'
complete -c komai -n '__komai_at_path' -a 'user'
complete -c komai -n '__komai_at_path app' -l help
complete -c komai -n '__komai_at_path app' -s h
complete -c komai -n '__komai_at_path app' -a 'api-version'
complete -c komai -n '__komai_at_path app' -a 'version'
complete -c komai -n '__komai_within app version' -l help
complete -c komai -n '__komai_within app version' -s h
complete -c komai -n '__komai_within app api-version' -l help
complete -c komai -n '__komai_within app api-version' -s h
complete -c komai -n '__komai_at_path completions' -l help
complete -c komai -n '__komai_at_path completions' -s h
complete -c komai -n '__komai_at_path completions' -a 'bash'
complete -c komai -n '__komai_at_path completions' -a 'fish'
complete -c komai -n '__komai_at_path completions' -a 'zsh'
complete -c komai -n '__komai_within completions bash' -l help
complete -c komai -n '__komai_within completions bash' -s h
complete -c komai -n '__komai_within completions zsh' -l help
complete -c komai -n '__komai_within completions zsh' -s h
complete -c komai -n '__komai_within completions fish' -l help
complete -c komai -n '__komai_within completions fish' -s h
complete -c komai -n '__komai_at_path media' -l help
complete -c komai -n '__komai_at_path media' -s h
complete -c komai -n '__komai_at_path media' -a 'fetch'
complete -c komai -n '__komai_at_path media' -a 'upload'
complete -c komai -n '__komai_within media fetch' -l help
complete -c komai -n '__komai_within media fetch' -s h
complete -c komai -n '__komai_within media upload' -l content-type
complete -c komai -n '__komai_within media upload' -l filename
complete -c komai -n '__komai_within media upload' -l help
complete -c komai -n '__komai_within media upload' -l stdin
complete -c komai -n '__komai_within media upload' -s h
complete -c komai -n '__komai_at_path mcp' -l help
complete -c komai -n '__komai_at_path mcp' -s h
complete -c komai -n '__komai_at_path mcp' -a 'serve'
complete -c komai -n '__komai_within mcp serve' -l access -xa 'read_only read_write'
complete -c komai -n '__komai_within mcp serve' -l help
complete -c komai -n '__komai_within mcp serve' -s h
complete -c komai -n '__komai_at_path profiles' -l help
complete -c komai -n '__komai_at_path profiles' -s h
complete -c komai -n '__komai_at_path profiles' -a 'launcher'
complete -c komai -n '__komai_at_path profiles launcher' -l help
complete -c komai -n '__komai_at_path profiles launcher' -s h
complete -c komai -n '__komai_at_path profiles launcher' -a 'create'
complete -c komai -n '__komai_at_path profiles launcher' -a 'remove'
complete -c komai -n '__komai_within profiles launcher create' -l help
complete -c komai -n '__komai_within profiles launcher create' -s h
complete -c komai -n '__komai_within profiles launcher remove' -l help
complete -c komai -n '__komai_within profiles launcher remove' -s h
complete -c komai -n '__komai_at_path rooms' -l help
complete -c komai -n '__komai_at_path rooms' -s h
complete -c komai -n '__komai_at_path rooms' -a 'ban'
complete -c komai -n '__komai_at_path rooms' -a 'create'
complete -c komai -n '__komai_at_path rooms' -a 'invite'
complete -c komai -n '__komai_at_path rooms' -a 'join'
complete -c komai -n '__komai_at_path rooms' -a 'kick'
complete -c komai -n '__komai_at_path rooms' -a 'leave'
complete -c komai -n '__komai_at_path rooms' -a 'list'
complete -c komai -n '__komai_at_path rooms' -a 'new-direct-chat'
complete -c komai -n '__komai_at_path rooms' -a 'send'
complete -c komai -n '__komai_at_path rooms' -a 'send-image'
complete -c komai -n '__komai_at_path rooms' -a 'timeline'
complete -c komai -n '__komai_at_path rooms' -a 'unban'
complete -c komai -n '__komai_within rooms list' -l help
complete -c komai -n '__komai_within rooms list' -s h
complete -c komai -n '__komai_within rooms timeline' -l before-event-id
complete -c komai -n '__komai_within rooms timeline' -l fetch-mode -xa 'cached_only server_fetch_if_needed'
complete -c komai -n '__komai_within rooms timeline' -l help
complete -c komai -n '__komai_within rooms timeline' -l include-unsigned-fields
complete -c komai -n '__komai_within rooms timeline' -l limit
complete -c komai -n '__komai_within rooms timeline' -s h
complete -c komai -n '__komai_within rooms join' -l help
complete -c komai -n '__komai_within rooms join' -s h
complete -c komai -n '__komai_within rooms new-direct-chat' -l help
complete -c komai -n '__komai_within rooms new-direct-chat' -s h
complete -c komai -n '__komai_within rooms create' -l alias-localpart
complete -c komai -n '__komai_within rooms create' -l creation-content
complete -c komai -n '__komai_within rooms create' -l direct
complete -c komai -n '__komai_within rooms create' -l encrypted
complete -c komai -n '__komai_within rooms create' -l help
complete -c komai -n '__komai_within rooms create' -l initial-state
complete -c komai -n '__komai_within rooms create' -l invite
complete -c komai -n '__komai_within rooms create' -l name
complete -c komai -n '__komai_within rooms create' -l power-levels
complete -c komai -n '__komai_within rooms create' -l preset -xa 'private_chat public_chat trusted_private_chat'
complete -c komai -n '__komai_within rooms create' -l public
complete -c komai -n '__komai_within rooms create' -l room-version
complete -c komai -n '__komai_within rooms create' -l space
complete -c komai -n '__komai_within rooms create' -l topic
complete -c komai -n '__komai_within rooms create' -s h
complete -c komai -n '__komai_within rooms invite' -l help
complete -c komai -n '__komai_within rooms invite' -l reason
complete -c komai -n '__komai_within rooms invite' -s h
complete -c komai -n '__komai_within rooms kick' -l help
complete -c komai -n '__komai_within rooms kick' -l reason
complete -c komai -n '__komai_within rooms kick' -s h
complete -c komai -n '__komai_within rooms ban' -l help
complete -c komai -n '__komai_within rooms ban' -l reason
complete -c komai -n '__komai_within rooms ban' -s h
complete -c komai -n '__komai_within rooms unban' -l help
complete -c komai -n '__komai_within rooms unban' -l reason
complete -c komai -n '__komai_within rooms unban' -s h
complete -c komai -n '__komai_within rooms leave' -l help
complete -c komai -n '__komai_within rooms leave' -l reason
complete -c komai -n '__komai_within rooms leave' -s h
complete -c komai -n '__komai_within rooms send' -l format -xa 'auto plain html'
complete -c komai -n '__komai_within rooms send' -l help
complete -c komai -n '__komai_within rooms send' -l msgtype -xa 'text notice'
complete -c komai -n '__komai_within rooms send' -s h
complete -c komai -n '__komai_within rooms send-image' -l caption
complete -c komai -n '__komai_within rooms send-image' -l filename
complete -c komai -n '__komai_within rooms send-image' -l help
complete -c komai -n '__komai_within rooms send-image' -s h
complete -c komai -n '__komai_at_path settings' -l help
complete -c komai -n '__komai_at_path settings' -s h
complete -c komai -n '__komai_at_path settings' -a 'ui'
complete -c komai -n '__komai_at_path settings ui' -l help
complete -c komai -n '__komai_at_path settings ui' -s h
complete -c komai -n '__komai_at_path settings ui' -a 'set-theme'
complete -c komai -n '__komai_at_path settings ui' -a 'theme'
complete -c komai -n '__komai_within settings ui theme' -l help
complete -c komai -n '__komai_within settings ui theme' -s h
complete -c komai -n '__komai_within settings ui set-theme' -l help
complete -c komai -n '__komai_within settings ui set-theme' -s h
complete -c komai -n '__komai_at_path theme' -l help
complete -c komai -n '__komai_at_path theme' -s h
complete -c komai -n '__komai_at_path theme' -a 'create-sample'
complete -c komai -n '__komai_at_path theme' -a 'list'
complete -c komai -n '__komai_at_path theme' -a 'tinted-import'
complete -c komai -n '__komai_at_path theme' -a 'tinted-search'
complete -c komai -n '__komai_within theme list' -l help
complete -c komai -n '__komai_within theme list' -s h
complete -c komai -n '__komai_within theme tinted-search' -l help
complete -c komai -n '__komai_within theme tinted-search' -s h
complete -c komai -n '__komai_within theme tinted-import' -l force
complete -c komai -n '__komai_within theme tinted-import' -l help
complete -c komai -n '__komai_within theme tinted-import' -l variant -xa 'light dark'
complete -c komai -n '__komai_within theme tinted-import' -s h
complete -c komai -n '__komai_within theme create-sample' -l force
complete -c komai -n '__komai_within theme create-sample' -l help
complete -c komai -n '__komai_within theme create-sample' -s h
complete -c komai -n '__komai_at_path user' -l help
complete -c komai -n '__komai_at_path user' -s h
complete -c komai -n '__komai_at_path user' -a 'device-id'
complete -c komai -n '__komai_at_path user' -a 'homeserver-url'
complete -c komai -n '__komai_at_path user' -a 'id'
complete -c komai -n '__komai_at_path user' -a 'set-status'
complete -c komai -n '__komai_at_path user' -a 'status'
complete -c komai -n '__komai_within user id' -l help
complete -c komai -n '__komai_within user id' -s h
complete -c komai -n '__komai_within user homeserver-url' -l help
complete -c komai -n '__komai_within user homeserver-url' -s h
complete -c komai -n '__komai_within user device-id' -l help
complete -c komai -n '__komai_within user device-id' -s h
complete -c komai -n '__komai_within user status' -l help
complete -c komai -n '__komai_within user status' -s h
complete -c komai -n '__komai_within user set-status' -l help
complete -c komai -n '__komai_within user set-status' -s h
