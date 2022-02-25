#!/usr/bin/env expect

if {$argc<2} {
  send_user "Usage: $argv0 sql_file database"
  exit
}

set sql_file [lindex $argv 0]
set database [lindex $argv 1]

spawn mysql -u root -p

expect "password:"
send "你的密码\r"

expect "mysql>"
send "create database $database CHARACTER SET utf8 COLLATE utf8_general_ci;\r"

expect "mysql>"
send "use $database;\r"

expect "mysql>"
send "source $sql_file;\r"

expect "mysql>"
send "show tables;\r"

expect "mysql>"
send "quit;\r"

expect eof
