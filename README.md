# tripping-nemesis

A collection of daemons written for (OpenBSD) [PF](http://www.openbsd.org/faq/pf/), that listen on [divert](http://www.openbsd.org/cgi-bin/man.cgi?query=divert&sektion=4) sockets.

```
WARNING: THESE TOOLS ARE EXPERIMENTAL AND IN NO-WAY PRODUCTION READY.

Feel free to test and run them on your systems, but make sure you keep a close eye.
```

List of diverters available:
  
  * `bofh-divert` Divert connections to this daemon and forever ban each src host. 
  * `dnsbl-divert` Divert connections to this daemon and check if the source ip is on a dnsbl and drop, or redirect to the legit owner. (still work-in-progress)

## Building

On an OpenBSD system, get the source and simply run make:

```
$ git clone https://github.com/echothrust/tripping-nemesis
$ cd tripping-nemesis
$ make
```

This will compile the binaries for the diverters. If you wish, you can manually copy the executables somewhere like `/usr/local/sbin`, but this is not a requirement.

<sub>Note: if git is not available in your system, you can always download the code as a zip file ([http link](https://github.com/echothrust/tripping-nemesis/archive/master.zip)).</sub>

## Running

### bofh-divert

A simple divert socket daemon that can used to automaticaly block connections. With the help of PF, you redirect a bunch of unused (by you) ports to this daemon listening on a divert socket and hosts that attempt access are instantly added to a predefined PF table. Combined with a block rule for that table, this essentially sets tripwires for any attackers probing those unused TCP ports, effectively blocking the rest of the attempts originating from the same IP addresses.

```
$ ./bofh-divert                                                            
usage: bofh-divert <divert_port> <pf_table_name>
        <divert_port> divert port number to bind (1-65535)
        <pf_table_name> table to add collected host IPs (up to 32 chars)
```

Say you run `bofh-divert 1100 bastards`, you would also need the corresponding PF rules for this to work, in `/etc/pf.conf`, say for a list of well-known scanner ports:

```
table <bastards> persist counters
block in log quick from <bastards>
pass in log quick on { egress } inet proto tcp from !<bastards> to port { 22, 23, 81, 138, 139, 445, 1024, 3389, 5900, 6001, 8080, 8888 } divert-packet port 1100 no state
```

All daemon activity is appropriately logged through syslog, e.g.:

```
Sep 17 18:56:16 fw01 bofh-divert: attacker_ip:13477 -> your_ip:3389
```

### dnsbl-divert

A similar daemon that can be used on firewalls to fence connections on listening (used) ports. Based on DNS blacklists, source IPs can be validated prior to allowing the connection to happen.

```
$ ./dnsbl-divert                                                           
usage: dnsbl-divert <divert_port> <pf_table_black> <pf_table_cache> [dns_ip]
        <divert_port>    divert port number to bind (1-65535)
        <pf_table_black> table to populate with DNSBLed hosts (up to 32 chars)
        <pf_table_cache> table to cache already-looked-up hosts (up to 32 chars)
        <dns_ip>         DNS server address (default: use system-configured dns)
```

This is BETA/untested software that can take numerous improvements. Usage is very similar to bofh-divert, but this is destined for application in front of listening ports. For up-to-date running instructions, PF config and also for setting your prefered DNSBLs, please take a look in the source code.

## Notes
  
The code is destined to compile and run on OpenBSD 5.3 amd64. It could also be suitable for other platforms featuring PF, but modifications may be needed. 

On OpenBSD, superuser privileges are required to open a divert socket (and thus run these programs).

When dealing with pf tables you also need write access to /dev/pf.

All the diverters require the pre-existance of the pf table.

## Contributing

There sure is room for improvement, but also many ideas on similar diverters to implement. Code contributions are always welcome:

1. [Fork it](https://github.com/echothrust/tripping-nemesis/fork)
2. Clone your forked project (`git clone https://github.com/YOUR-ACCOUNT/tripping-nemesis`)
3. (Optional) Create your feature branch (`git checkout -b my-new-feature`)
4. Add code as you see fit (introduce new files with `git add my-new-feature.c`)
5. (Optional) Commit your changes (`git commit -am 'Add some feature'`)
6. Push back to your forked clone (`git push` or `git push origin my-new-feature`)
7. Create new Pull Request [![Info](https://help.github.com/assets/help/info-icon-ba11a61a3770bbc50703f444d80e915b.png "Creating a new Pull Request")](https://help.github.com/articles/creating-a-pull-request)
