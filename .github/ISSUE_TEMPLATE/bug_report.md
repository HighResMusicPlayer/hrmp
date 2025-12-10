---
name: Bug report
about: Create a report to help us improve
title: ''
labels: bug
assignees: ''

---

**Describe the bug**

A clear and concise description of what the bug is.

**To Reproduce**

Steps to reproduce the behavior.

**Version**

What is the version of hrmp ?

Please, use `hrmp -V`

**ALSA**

What is the version of ALSA ?

Please, use `rpm -qa | grep alsa | sort` or similar

**libsndfile**

What is the version of libsndfile ?

Please, use `rpm -qa | grep libsndfile | sort` or similar

**opus**

What is the version of opus ?

Please, use `rpm -qa | grep opus | sort` or similar

**faad2**

What is the version of faad2 ?

Please, use `rpm -qa | grep  | sort` or similar

**gtk3**

What is the version of gtk3 ?

Please, use `rpm -qa | grep  | sort` or similar

**OS**

Which Operating System (OS) is used ?

Please, use

```
cat /etc/system-release
cat /proc/version
```

**Devices**

Can you provide information about the devices ?

Please, use `hrmp -s`

**Configuration**

Can you provide the configuration of hrmp ?

Please, use `cat ~/.hrmp/hrmp.conf`

**Debug logs**

Can you provide any debug logs (`log_level = debug5`) of the issue ?

**Tip**

Use \`\`\` before and after the text to keep the output as is.
