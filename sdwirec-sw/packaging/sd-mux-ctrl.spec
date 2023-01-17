%define rc_version 0

%if 0%{?rc_version}
%define release_prefix 0.rc%{rc_version}.
%endif

Name:           sd-mux-ctrl
Summary:        Control software for sd-mux devices.
Version:        0.0.2
Release:        %{?release_prefix}%{?opensuse_bs:<CI_CNT>.<B_CNT>}%{!?opensuse_bs:0}
Group:          Development/Tools
License:        Apache-2.0
URL:            http://www.tizen.org
Source0:        %{name}_%{version}.tar.gz
BuildRequires:  cmake
Requires:       libftdi >= 1.2
Requires:       popt
Requires:       awk

BuildRoot:  %{_tmppath}/%{name}_%{version}-build

%description
Tool for controlling multiple sd-mux devices.
 This tool allows:
  to connect SD card to DUT (Device Under Test) or to TS (Test Server)
  to connect one USB port to DUT or TS
  to power off or on DUT
  to reset DUT through power disconnecting and reconnecting


%prep
%setup -q -n %{name}-%{version}

%build
cmake -DCMAKE_INSTALL_PREFIX=/usr
%__make

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}/%{_mandir}/man1
install -m644 docs/man/%{name}.1 %{buildroot}/%{_mandir}/man1
install -d -m0755 %{buildroot}/%{_sysconfdir}/bash_completion.d/
install -Dp -m0755 etc/bash_completion.d/%{name} %{buildroot}/%{_sysconfdir}/bash_completion.d/

%files
%{_bindir}/%{name}
%{_mandir}/man1/*
%{_sysconfdir}/bash_completion.d/*
