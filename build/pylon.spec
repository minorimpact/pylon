Name: pylon
Version: 0.0.42 
Release: 2%{?dist}
Summary: Fast graphing service
Epoch: 0

Group: Applications/System
License: GPL
URL: http://ffn.com
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root

BuildRequires: gcc, libev-devel	

Provides: pylon

%define  debug_package %{nil}

%description
Fast graphing service.

%prep
%setup

%build
make build 

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p ${RPM_BUILD_ROOT} 
make install_rpm RPM_BUILD_ROOT=${RPM_BUILD_ROOT}

%clean
rm -rf $RPM_BUILD_ROOT

%check
rm -rf ./debugfiles.list ./debuglinks.list ./debugsources.list

%files
%defattr(-,root,root)
/etc/rc.d/init.d/pylon
/usr/local/bin/pylon
/usr/local/bin/pylonstatus.pl
/usr/local/man/man8/pylon.8.gz
/usr/local/man/man8/pylonstatus.pl.8.gz

%changelog
* Thu Apr 14 2016 <ahall@ffn.com> 0.0.42-1
- initial packaging

