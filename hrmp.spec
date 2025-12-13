Name:          hrmp
Version:       0.11.1
Release:       1%{dist}
Summary:       High-Resolution Music Player
License:       GPL
URL:           https://github.com/HighResMusicPlayer/hrmp
Source0:       https://github.com/HighResMusicPlayer/hrmp/releases/download/%{version}/hrmp-%{version}.tar.gz

BuildRequires: gcc cmake make python3-docutils alsa-lib alsa-lib-devel libsndfile libsndfile-devel opus-devel faad2-devel gtk3-devel
Requires:      alsa-lib libsndfile opus faad2 gtk3

%description
hrmp is a high resolution music player.

%prep
%setup -q

%build

%{__mkdir} build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
%{__make}

%install

%{__mkdir} -p %{buildroot}%{_sysconfdir}
%{__mkdir} -p %{buildroot}%{_bindir}
%{__mkdir} -p %{buildroot}%{_libdir}
%{__mkdir} -p %{buildroot}%{_mandir}/man1
%{__mkdir} -p %{buildroot}%{_mandir}/man5
%{__mkdir} -p %{buildroot}%{_sysconfdir}/hrmp

%{__install} -m 644 %{_builddir}/%{name}-%{version}/LICENSE %{buildroot}%{_docdir}/%{name}/LICENSE
%{__install} -m 644 %{_builddir}/%{name}-%{version}/CODE_OF_CONDUCT.md %{buildroot}%{_docdir}/%{name}/CODE_OF_CONDUCT.md
%{__install} -m 644 %{_builddir}/%{name}-%{version}/CONTRIBUTING.md %{buildroot}%{_docdir}/%{name}/CONTRIBUTING.md
%{__install} -m 644 %{_builddir}/%{name}-%{version}/README.md %{buildroot}%{_docdir}/%{name}/README.md
%{__install} -m 644 %{_builddir}/%{name}-%{version}/doc/CONFIGURATION.md %{buildroot}%{_docdir}/%{name}/CONFIGURATION.md
%{__install} -m 644 %{_builddir}/%{name}-%{version}/doc/DEVELOPERS.md %{buildroot}%{_docdir}/%{name}/DEVELOPERS.md
%{__install} -m 644 %{_builddir}/%{name}-%{version}/doc/GETTING_STARTED.md %{buildroot}%{_docdir}/%{name}/GETTING_STARTED.md
%{__install} -m 644 %{_builddir}/%{name}-%{version}/doc/CLI.md %{buildroot}%{_docdir}/%{name}/CLI.md
%{__install} -m 644 %{_builddir}/%{name}-%{version}/doc/PR_GUIDE.md %{buildroot}%{_docdir}/%{name}/PR_GUIDE.md
%{__install} -m 644 %{_builddir}/%{name}-%{version}/doc/RPM.md %{buildroot}%{_docdir}/%{name}/RPM.md

%{__install} -m 644 %{_builddir}/%{name}-%{version}/build/doc/hrmp.1 %{buildroot}%{_mandir}/man1/hrmp.1
%{__install} -m 644 %{_builddir}/%{name}-%{version}/build/doc/hrmp-ui.1 %{buildroot}%{_mandir}/man1/hrmp-ui.1
%{__install} -m 644 %{_builddir}/%{name}-%{version}/build/doc/hrmp.conf.5 %{buildroot}%{_mandir}/man5/hrmp.conf.5

%{__install} -m 755 %{_builddir}/%{name}-%{version}/build/src/hrmp %{buildroot}%{_bindir}/hrmp
%{__install} -m 755 %{_builddir}/%{name}-%{version}/build/src/hrmp-ui %{buildroot}%{_bindir}/hrmp-ui

%{__install} -m 755 %{_builddir}/%{name}-%{version}/build/src/libhrmp.so.%{version} %{buildroot}%{_libdir}/libhrmp.so.%{version}

chrpath -r %{_libdir} %{buildroot}%{_bindir}/hrmp

cd %{buildroot}%{_libdir}/
%{__ln_s} -f libhrmp.so.%{version} libhrmp.so.0
%{__ln_s} -f libhrmp.so.0 libhrmp.so

%files
%license %{_docdir}/%{name}/LICENSE
%{_docdir}/%{name}/CLI.md
%{_docdir}/%{name}/CODE_OF_CONDUCT.md
%{_docdir}/%{name}/CONFIGURATION.md
%{_docdir}/%{name}/CONTRIBUTING.md
%{_docdir}/%{name}/DEVELOPERS.md
%{_docdir}/%{name}/GETTING_STARTED.md
%{_docdir}/%{name}/PR_GUIDE.md
%{_docdir}/%{name}/README.md
%{_docdir}/%{name}/RPM.md
%{_mandir}/man1/hrmp.1*
%{_mandir}/man1/hrmp-ui.1*
%{_mandir}/man5/hrmp.conf.5*
%{_bindir}/hrmp
%{_bindir}/hrmp-ui
%{_libdir}/libhrmp.so
%{_libdir}/libhrmp.so.0
%{_libdir}/libhrmp.so.%{version}

%changelog
