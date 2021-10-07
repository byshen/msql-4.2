Summary: The Mini SQL Database System
Name: msql
Version: @VERSION@
Release: @RELEASE@
Copyright: Copyright Hughes Technologies Pty Ltd
Group: Application/Database
Source: @FILE@.tar

%description
The Mini SQL database system is a complete relational database package
supporting a subset of the ANSI SQL standard as it's query language.  The
package operates in a client/server method on any TCP/IP network.

%prep
%setup
%build
./setup
make all
%install
rm -rf /usr/local/msql3
make install
%clean
%files
/usr/local/msql3
