#!/usr/bin/perl
#
# alarm.cgi - sample CGI script to display NX-8 alarm panel status
#
use CGI;

my $LOCALCSS = <<EOF;
#box1 {
  background: #d8d8d8;
  margin:16px;
  padding: 10px 10px;
  border:1px solid;
  border-radius:10px;
  box-shadow: 5px 6px 5px #aaaaaa;
  -moz-border-radius:10px;
}
EOF

$q = CGI->new;            
print $q->header, $q->start_html(-title=>'NX-8V2: Alarm Panel Status',
				 -style=>{-code=>$LOCALCSS},
    );


print "<table id=box1><tr><td><pre>\n";
$r=system("/usr/local/bin/nxstat -Z 2>&1");
if ($r != 0) {
    print("failed to run nxstat<br>"); 
}
print "</pre></td></tr></table>\n";


print "<table id=box1><tr><td>Log:<br><font size=-1><pre>\n";
$r=system("tail -20 /var/log/nxgipd.log");
print "<pre></font></td></tr></table>\n";


print "<font size=-1>".localtime(time())."</font>\n";

print $q->end_html;

# eof :-)
