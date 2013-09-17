tripping-nemesis
================

A collection of (OpenBSD) PF divert socket daemons

```
WARNING: THESE ARE TOOLS ARE EXPERIMENTAL AND IN NO-WAY PRODUCTION READY.

Feel free to test and run them on your systems, but make sure you keep a close eye.
```

List of diverters available
  
  * `bofh-divert` Divert connections to this daemon and forever ban each src host. 
  * `dnsbl-divert` Divert connections to this daemon and check if the source ip is on a dnsbl and drop, or redirect to the legit owner. (still work-in-progress)
  
On OpenBSD superuser privileges are required to open a divert socket (and thus run these programs).
When dealing with pf tables you also need write access to /dev/pf.


All the diverters require the pre-existance of the pf table.