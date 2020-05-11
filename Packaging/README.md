# RPM packaging for clksignal
This simple Ansible playbook creates and installs an RPM package of the current release of clksignal

If the version that you build is newer than what you have installed, it will be automatically upgraded.

## Usage

`ansible-playbook main.yml -K
