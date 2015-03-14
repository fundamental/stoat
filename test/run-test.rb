#!/usr/bin/ruby
#
# stoat - LLVM Based Static Analysis Tool
# Copyright (C) 2015 Mark McCurry
#
# This file is part of stoat.
#
# stoat is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# stoat is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with stoat.  If not, see <http://www.gnu.org/licenses/>.
#
# Argument:
# 1      - path to run.rb
# 2      - path to libfoo.so
# 3      - compiler to use
# 4      - whitelist to use
# 4..end - path to source files

if ARGV.length < 5
    $stderr.puts "Too Few Arguments"
    error 1
end

deductions  = ARGV[0]
llvm_lib    = ARGV[1]
compiler    = ARGV[2]
whitelist   = ARGV[3]
blacklist   = ARGV[4]
suppression = ARGV[5]


if(suppression != "nil")
    suppression = "-s #{suppression}"
else
    suppression = ""
end

args = ""
if(/\+\+/.match compiler)
    args = "-std=c++11" #Show off inlining
else
    args = "-std=c11"
end

source_files = ARGV[6...ARGV.length].join " "

`mkdir tmp-build-dir`
Dir.chdir 'tmp-build-dir'
`#{compiler} #{args} -emit-llvm -c #{source_files}`
puts `ruby #{deductions} --llvm-passes #{llvm_lib} -w #{whitelist} -b #{blacklist} #{suppression} -r .`
return_code = $?.to_i
Dir.chdir '..'
`rm -r tmp-build-dir`

puts "return code = '#{return_code}'"

if(return_code == 0)
    exit(0)
else
    exit(-1)
end