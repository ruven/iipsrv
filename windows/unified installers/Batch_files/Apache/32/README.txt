Two batch files are called after the installation (immediately after files were copied),
to configure everything (set memcached, create url shortcuts, configure Transcoder).
apache-conf.bat is called after appropriate wizard page before files are copied.
junction.exe is needed because Win XP doesn't have good function for this. 
For closer information read documentation and comments in batch files and also in scripts.