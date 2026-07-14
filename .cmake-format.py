with section("format"):
    line_width = 120
    tab_size = 4
    line_ending = "unix"
    command_case = "canonical"
    keyword_case = "unchanged"

with section("lint"):
    # Existing public/internal CMake variables use several established naming
    # conventions. Keep structural diagnostics strict without relabeling the
    # build API as part of lint adoption.
    disabled_codes = ["C0103"]

