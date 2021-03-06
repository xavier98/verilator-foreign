#!/usr/bin/perl
if (!$::Driver) { use FindBin; exec("$FindBin::Bin/bootstrap.pl", @ARGV, $0); die; }
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# Copyright 2008 by Wilson Snyder. This program is free software; you can
# redistribute it and/or modify it under the terms of either the GNU
# Lesser General Public License Version 3 or the Perl Artistic License
# Version 2.0.

$Self->{vlt} or $Self->skip("Verilator only test");

compile (
    v_flags2 => ["--lint-only"],
    fails=>1,
    verilator_make_gcc => 0,
    make_top_shell => 0,
    make_main => 0,
    expect=> quotemeta(
qq{%Error: t/t_lint_pkg_colon_bad.v:6: syntax error, unexpected ::, expecting ')' or ','
%Error: t/t_lint_pkg_colon_bad.v:6: Perhaps 'mispkg' is a package which needs to be predeclared? (IEEE 2012 26.3)
%Error: t/t_lint_pkg_colon_bad.v:7: syntax error, unexpected ::, expecting ',' or ';'
%Error: t/t_lint_pkg_colon_bad.v:7: Perhaps 'mispkgb' is a package which needs to be predeclared? (IEEE 2012 26.3)
}).'%Error: Exiting due to.*'
    );

ok(1);
1;
