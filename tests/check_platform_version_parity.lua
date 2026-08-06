-- Every platform section of a package must carry the same set of versions.
--
-- WHY THIS EXISTS
--
-- A version bump is a one-line-looking edit that has to land in N places.
-- Editing `xpm.linux` and reading the file back gives a file that contains
-- the new version -- so the author, and any whole-file grep, sees success.
-- The other platforms are simply left behind, and the failure surfaces
-- somewhere else entirely: `mcpplibs.xpkg@0.0.53 not found` on macOS,
-- against a file that literally contains `["0.0.53"]`.
--
-- Measured 2026-08-06: xpkg 0.0.52 and 0.0.53 were both added to `xpm.linux`
-- alone. Linux CI went green twice. macOS and Windows failed, and the cause
-- was diagnosed eight times from the outside -- index commit, published
-- artifact, rolling pointer, releases/latest, publish lag, the client version
-- pin, install-vs-use, the lockfile hash -- each of which was correct,
-- because each asked "is 0.0.53 in the index?" rather than "is it in THIS
-- PLATFORM's section?".
--
-- THE RULE
--
--   Within one descriptor, every platform section that carries any version
--   entry must carry the same set of version entries.
--
-- Packages that genuinely diverge (a platform that stopped at an older
-- release, or ships versions the others never had) opt out explicitly:
--
--   package = {
--       ...
--       -- <reason the platforms differ>
--       platform_versions_diverge = true,
--   }
--
-- The opt-out is a claim someone made on purpose, which is the difference
-- between a divergence and an omission. Surveyed at the time this was
-- written: 80 descriptors, 0 divergences -- so the rule costs nothing today
-- and only fires on a partial bump.
--
-- Usage: lua5.4 tests/check_platform_version_parity.lua <file.lua> [...]
-- Exits non-zero, with ::error lines, listing what each platform is missing.

function import(...)
    return setmetatable({}, {__index = function() return function() end end})
end

local fail = 0

local function err(file, msg)
    io.stderr:write(string.format("::error file=%s::%s\n", file, msg))
    fail = 1
end

local function version_keys(tbl)
    local set, n = {}, 0
    for k in pairs(tbl) do
        -- A version key, not `latest` / `deps` / `exports` / ...
        if type(k) == "string" and k:match("^%d[%d%.]*$") then
            set[k] = true
            n = n + 1
        end
    end
    return set, n
end

local function sorted(set)
    local out = {}
    for k in pairs(set) do table.insert(out, k) end
    table.sort(out)
    return out
end

local function check(file)
    package = nil
    local chunk = loadfile(file, "t")
    if not chunk then return end                    -- parse errors: other checks
    if not pcall(chunk) then return end
    local p = package
    if type(p) ~= "table" or type(p.xpm) ~= "table" then return end
    if p.platform_versions_diverge then return end

    -- Only platforms that carry versions at all. A section that is purely
    -- `inherits` or `deps` is not an omission.
    local plats, sets = {}, {}
    for plat, pt in pairs(p.xpm) do
        if type(pt) == "table" then
            local s, n = version_keys(pt)
            if n > 0 then
                table.insert(plats, plat)
                sets[plat] = s
            end
        end
    end
    if #plats < 2 then return end
    table.sort(plats)

    local union = {}
    for _, plat in ipairs(plats) do
        for v in pairs(sets[plat]) do union[v] = true end
    end

    for _, plat in ipairs(plats) do
        local missing = {}
        for _, v in ipairs(sorted(union)) do
            if not sets[plat][v] then table.insert(missing, v) end
        end
        if #missing > 0 then
            err(file, string.format(
                "xpm.%s is missing %s -- present in another platform's section. "
                .. "A version bump has to land in every platform block; editing "
                .. "one leaves a file that still contains the version, so the "
                .. "omission reads as 'not found' on the platforms that lack it. "
                .. "If the platforms genuinely differ, set "
                .. "`platform_versions_diverge = true` and say why.",
                plat, table.concat(missing, ", ")))
        end
    end
end

if #arg == 0 then
    io.stderr:write("usage: check_platform_version_parity.lua <file.lua> [...]\n")
    os.exit(2)
end
for _, file in ipairs(arg) do check(file) end
os.exit(fail)
