-- A version number, once published, names one set of bytes forever.
--
-- WHY THIS EXISTS
--
-- Every package in this index is published as an archive of a repository at a
-- tag, so ANY change to that repository changes the artefact. A release
-- therefore needs the version in `mcpp.toml` bumped, and nothing checks that
-- it was: a package's own CI validates the tree in front of it and has no
-- opinion about which numbers are already taken.
--
-- Measured 2026-08-25, following openkal 0.7.0 through the ecosystem: SIX of
-- the eight repositories had a branch ready to merge whose version equalled
-- the version on `main`, which is the version already in this index. Two of
-- them had gained a whole interface implementation. Every one of those
-- packages was green.
--
-- What happens next is quiet rather than loud. `git tag 0.5.3` finds the tag
-- present and succeeds; the archive fetched from that tag is the OLD content;
-- both mirror legs then agree with each other and with the source archive,
-- because all three are the same old bytes. The release verifies perfectly
-- and publishes nothing. The only surviving trace is a second `["0.5.3"]`
-- entry here -- and Lua accepts a repeated key in a table constructor, keeps
-- the last one, and reports nothing.
--
-- THE RULE
--
--   Within one platform section, a version key appears at most once.
--
-- The check is textual and deliberately so. Loading the descriptor is exactly
-- what cannot see this: by the time the table exists, the duplicate has
-- already collapsed into the survivor.
--
-- Usage: lua5.4 tests/check_duplicate_versions.lua <file.lua> [...]
-- Exits non-zero, with ::error lines naming the file, the line, and the
-- platform section the repeat is in.

local failed = false

-- A platform section opens with `<name> = {` at the indentation the
-- descriptors use inside `xpm`, and a version key is `["<digits and dots>"]`.
-- Anything else -- `url`, `sha256`, nested tables -- is passed over.
local function check(path)
    local handle = io.open(path, "r")
    if not handle then
        io.stderr:write(("::error file=%s::cannot be read\n"):format(path))
        failed = true
        return
    end

    local section, seen, line_no = nil, {}, 0
    for line in handle:lines() do
        line_no = line_no + 1

        local platform = line:match("^%s%s%s%s%s%s%s%s([%w_]+)%s*=%s*{%s*$")
        if platform then
            section, seen = platform, {}
        end

        local version = line:match('^%s*%["([%d][%d%.]*)"%]%s*=')
        if version and section then
            if seen[version] then
                io.stderr:write(("::error file=%s,line=%d::%s appears twice in the %s section (first at line %d). A published version names one set of bytes; bump it instead.\n")
                    :format(path, line_no, version, section, seen[version]))
                failed = true
            else
                seen[version] = line_no
            end
        end
    end
    handle:close()
end

if #arg == 0 then
    io.stderr:write("usage: check_duplicate_versions.lua <file.lua> [...]\n")
    os.exit(2)
end
for _, path in ipairs(arg) do check(path) end
os.exit(failed and 1 or 0)
