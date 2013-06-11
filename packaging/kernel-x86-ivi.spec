#
# Spec written for Tizen Mobile, some bits and pieces originate
# from MeeGo/Moblin/Fedora
#

%define upstream_version 3.13.3
%define variant x86-ivi
%define kernel_version %{version}-%{release}
%define kernel_full_version %{version}-%{release}-%{variant}
%define kernel_arch i386
%define kernel_arch_subdir arch/x86

Name: kernel-%{variant}
Summary: The Linux kernel
Group: System/Kernel
License: GPL-2.0
URL: http://www.kernel.org/
Version: %{upstream_version}

# The below is used when we are on an -rc version
#%#define rc_num 6
#%#define release_ver 0
#%#define rc_str %{?rc_num:0.rc%{rc_num}}%{!?rc_num:1}
#%if ! 0%{?opensuse_bs}
#Release: %{rc_str}.%{release_ver}.0.0
#%else
#Release: %{rc_str}.%{release_ver}.<CI_CNT>.<B_CNT>
#%endif
Release: 3

BuildRequires: module-init-tools
BuildRequires: findutils
BuildRequires: libelf-devel
BuildRequires: binutils-devel
BuildRequires: which
BuildRequires: bc
# net-tools provides the 'hostname' utility which kernel build wants
BuildRequires: net-tools
# The below is required for building perf
BuildRequires: flex
BuildRequires: bison
BuildRequires: libdw-devel
BuildRequires: python-devel
ExclusiveArch: %{ix86}

Provides: kernel = %{version}-%{release}
Provides: kernel-uname-r = %{kernel_full_version}

# We use 'setup-ivi-bootloader-conf' in post and postun, so ideally we need to
# have the below here, but this causes gbs/obs build failures like this:
#    "have choice for virtual-setup-ivi-bootloader needed by kernel-x86-ivi: setup-extlinux setup-gummiboot"
# The reason is that it will try to install the kernel to the build root, and
# fail with the above error. To fix it one would need to add 'setup-extlinux'
# or 'setup-gummiboot' to 'review.tizen.org/gerrit/scm/meta/build-config'. But
# it is probably not worth the trouble, so I commented out the below two lines.
# -- Artem
#Requires(post): virtual-setup-ivi-bootloader
#Requires(postun): virtual-setup-ivi-bootloader

# We can't let RPM do the dependencies automatic because it'll then pick up
# a correct but undesirable perl dependency from the module headers which
# isn't required for the kernel proper to function
AutoReq: no
AutoProv: yes

Source0: %{name}-%{version}.tar.bz2


%description
This package contains the Tizen IVI Linux kernel


%package devel
Summary: Development package for building kernel modules to match the %{variant} kernel
Group: Development/System
Provides: kernel-devel = %{kernel_full_version}
Provides: kernel-devel-uname-r = %{kernel_full_version}
Requires(post): /usr/bin/find
Requires: %{name} = %{version}-%{release}
AutoReqProv: no

%description devel
This package provides kernel headers and makefiles sufficient to build modules
against the %{variant} kernel package.


%package -n perf
Summary: The 'perf' performance counter tool
Group: System Environment/Kernel
Provides: perf = %{kernel_full_version}
Requires: %{name} = %{version}-%{release}

%description -n perf
This package provides the "perf" tool that can be used to monitor performance
counter events as well as various kernel internal events.



###
### PREP
###
%prep
# Unpack the kernel tarbal
%setup -q -n %{name}-%{version}



###
### BUILD
###
%build
# Make sure EXTRAVERSION says what we want it to say
sed -i "s/^EXTRAVERSION.*/EXTRAVERSION = -%{release}-%{variant}/" Makefile

# Build perf
make -s -C tools/lib/traceevent ARCH=%{kernel_arch} %{?_smp_mflags}
make -s -C tools/perf WERROR=0 ARCH=%{kernel_arch}

# Build kernel and modules
make -s ARCH=%{kernel_arch} ivi_defconfig
make -s ARCH=%{kernel_arch} %{?_smp_mflags} bzImage
make -s ARCH=%{kernel_arch} %{?_smp_mflags} modules



###
### INSTALL
###
%install
install -d %{buildroot}/boot

install -m 644 .config %{buildroot}/boot/config-%{kernel_full_version}
install -m 644 System.map %{buildroot}/boot/System.map-%{kernel_full_version}
install -m 755 %{kernel_arch_subdir}/boot/bzImage %{buildroot}/boot/vmlinuz-%{kernel_full_version}
# Dummy initrd, will not be included in the actual package but needed for files
touch %{buildroot}/boot/initrd-%{kernel_full_version}.img

make -s ARCH=%{kernel_arch} INSTALL_MOD_PATH=%{buildroot} modules_install KERNELRELEASE=%{kernel_full_version}
make -s ARCH=%{kernel_arch} INSTALL_MOD_PATH=%{buildroot} vdso_install KERNELRELEASE=%{kernel_full_version}
rm -rf %{buildroot}/lib/firmware

# And save the headers/makefiles etc for building modules against
#
# This all looks scary, but the end result is supposed to be:
# * all arch relevant include/ files
# * all Makefile/Kconfig files
# * all script/ files

# Remove existing build/source links and create pristine dirs
rm %{buildroot}/lib/modules/%{kernel_full_version}/build
rm %{buildroot}/lib/modules/%{kernel_full_version}/source
install -d %{buildroot}/lib/modules/%{kernel_full_version}/build
ln -s build %{buildroot}/lib/modules/%{kernel_full_version}/source

# First, copy all dirs containing Makefile of Kconfig files
cp --parents `find  -type f -name "Makefile*" -o -name "Kconfig*"` %{buildroot}/lib/modules/%{kernel_full_version}/build
install Module.symvers %{buildroot}/lib/modules/%{kernel_full_version}/build/
install System.map %{buildroot}/lib/modules/%{kernel_full_version}/build/

# Then, drop all but the needed Makefiles/Kconfig files
rm -rf %{buildroot}/lib/modules/%{kernel_full_version}/build/Documentation
rm -rf %{buildroot}/lib/modules/%{kernel_full_version}/build/scripts
rm -rf %{buildroot}/lib/modules/%{kernel_full_version}/build/include

# Copy config and scripts
install .config %{buildroot}/lib/modules/%{kernel_full_version}/build/
cp -a scripts %{buildroot}/lib/modules/%{kernel_full_version}/build
if [ -d %{kernel_arch_subdir}/scripts ]; then
    cp -a %{kernel_arch_subdir}/scripts %{buildroot}/lib/modules/%{kernel_full_version}/build/%{kernel_arch_subdir}/ || :
fi
if [ -f %{kernel_arch_subdir}/*lds ]; then
    cp -a %{kernel_arch_subdir}/*lds %{buildroot}/lib/modules/%{kernel_full_version}/build/%{kernel_arch_subdir}/ || :
fi
rm -f %{buildroot}/lib/modules/%{kernel_full_version}/build/scripts/*.o
rm -f %{buildroot}/lib/modules/%{kernel_full_version}/build/scripts/*/*.o
cp -a --parents %{kernel_arch_subdir}/include %{buildroot}/lib/modules/%{kernel_full_version}/build

# Copy include files
mkdir -p %{buildroot}/lib/modules/%{kernel_full_version}/build/include
find include/ -mindepth 1 -maxdepth 1 -type d | xargs -I{} cp -a {} %{buildroot}/lib/modules/%{kernel_full_version}/build/include

# Save the vmlinux file for kernel debugging into the devel package
cp vmlinux %{buildroot}/lib/modules/%{kernel_full_version}

# Mark modules executable so that strip-to-file can strip them
find %{buildroot}/lib/modules/%{kernel_full_version} -name "*.ko" -type f | xargs --no-run-if-empty chmod 755

# Move the devel headers out of the root file system
install -d %{buildroot}/usr/src/kernels
mv %{buildroot}/lib/modules/%{kernel_full_version}/build %{buildroot}/usr/src/kernels/%{kernel_full_version}

ln -sf /usr/src/kernels/%{kernel_full_version} %{buildroot}/lib/modules/%{kernel_full_version}/build

# Install perf
install -d %{buildroot}
make -s -C tools/perf DESTDIR=%{buildroot} install
install -d  %{buildroot}/usr/bin
install -d  %{buildroot}/usr/libexec
mv %{buildroot}/bin/* %{buildroot}/usr/bin/
mv %{buildroot}/libexec/* %{buildroot}/usr/libexec/
rm %{buildroot}/etc/bash_completion.d/perf



###
### SCRIPTS
###

%post
# "/etc/installerfw-environment" does not exist in MIC environment, when it
# builds the image. MIC will add boot-loader entries later using the
# 'setup-ivi-boot' script.
if [ -f "/etc/installerfw-environment" ] && \
   [ -x "/usr/sbin/setup-ivi-bootloader-conf" ]; then
	/usr/sbin/setup-ivi-bootloader-conf add -f vmlinuz-%{kernel_full_version}
	/usr/sbin/setup-ivi-bootloader-conf default -f vmlinuz-%{kernel_full_version}
fi

%post devel
if [ -x /usr/sbin/hardlink ]; then
	cd /usr/src/kernels/%{kernel_full_version}
	/usr/bin/find . -type f | while read f; do
		hardlink -c /usr/src/kernels/*/$f $f
	done
fi

%postun
if [ -f "/etc/installerfw-environment" ] && \
   [ -x "/usr/sbin/setup-ivi-bootloader-conf" ]; then
	/usr/sbin/setup-ivi-bootloader-conf remove -f vmlinuz-%{kernel_full_version}
fi


###
### FILES
###
%files
%license COPYING
/boot/vmlinuz-%{kernel_full_version}
/boot/System.map-%{kernel_full_version}
/boot/config-%{kernel_full_version}
%dir /lib/modules/%{kernel_full_version}
/lib/modules/%{kernel_full_version}/kernel
/lib/modules/%{kernel_full_version}/build
/lib/modules/%{kernel_full_version}/source
/lib/modules/%{kernel_full_version}/vdso
/lib/modules/%{kernel_full_version}/modules.*
%ghost /boot/initrd-%{kernel_full_version}.img


%files devel
%license COPYING
%verify(not mtime) /usr/src/kernels/%{kernel_full_version}
/lib/modules/%{kernel_full_version}/vmlinux


%files -n perf
%license COPYING
/usr/bin/perf
/usr/bin/trace
/usr/libexec/perf-core
