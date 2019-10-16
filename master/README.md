Run 'make' to build to tool. It runs as a few threads, one for receive, one for trasnmit and one for timers.

<pre>
Command line options:
-H define the port where the modem is connected to.
-s run as a secundary master
-l run in listen only mode
-U perform timed transmits instead of checking transmit ended
-C define an alternative config file (default: ~/.hmt_config and ./.hmt_config)
-B listen to burst mode devices only
-d run as line mode dumper
-T use table instead of database for information about frame layouts ('tf_tablefile' in configfile)
</pre>

As many USB<->serial dongles have limited provision to check if a transmission is done completely the -U
option comes in handy. 


