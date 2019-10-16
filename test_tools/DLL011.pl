#!/usr/bin/perl
# -w


#
# try to emulate the DLL011 test as good as possible
#
# it tries to indentity the DUT first
#
# then it sends a long request frame with a CRC error in a byte
#
# the location of the error is shifted on each cycle
#

use     Time::HiRes "usleep";

my 	$cmdline;
my 	$line;
my 	$i;
my 	$result;

$port = $ARGV[0];
if (!length ($port)){
	printf STDERR "usage: DLL11.pl <portname>\n";
	return 1;
}

#
# identity
#

# open first (for read), let harttestframe control handshake
		(<>); # eat unwanted chars from keyboard

		open (RESPONSE, "<$port")|| die " can't open port $port\n";

# open port, set handshake, write, reset handshake.....
        $cmdline=("harttestframe -H $port -S \"ff ff ff ff ff 02 00 00 00\"");               # set supply at 24VDC
        open(SCMD, "$cmdline |");
# let response collect in buffer
		usleep(500000);
# read buffer
		sysread(RESPONSE, $line, 1024, 0);
		close RESPONSE;
        close(SCMD);

		$array = unpack('H*', $line);
		my $len = length($array);
# unpacked
		for($i=0; $i < $len; ){
			$result = $result.substr($array, $i , 2).",";
			$i += 2;
		}
# made string, search it
		$len = length($result);
		for($i = 0; $i < $len; ){
			$val = sprintf  "%02s", substr($result,$i,2);
			if($val eq '06'){		# find response cmd 0
				goto LABEL1;
			}
			$i+= 3;
		}
# found SOH
LABEL1:		

		$address = substr($result,$i+21,6).substr($result,$i + 45 ,9);
		print "Found device $address, now send command 9 with data f4 f5 f6 f7\n";

#
# loop now for each byte in the long request frame
#
		for($loop = 1; $loop <= 18; $loop++){   # up and into checksum
			$line = "";
			$result = "";
			$array = "";
	
			open (RESPONSE, "<$port")|| die " can't open port\n";
			$cmdline = sprintf "./harttestframe -H $port -S \"ff ff ff ff ff 82 %s09 04 f4 f5 f6 f7\" -E $loop", $address;
			printf "Send to DUT, CRC error in byte %02d : $cmdline\n", $loop;
   		    open(SCMD, "$cmdline |");
# print location indicator
			for ($sloop = 0; $sloop < (57 + length($port)); $sloop++){
				printf " ";
			}
			for ($sloop = 0 ; $sloop < $loop; $sloop++){
				printf "   ";
			}
			printf "^\n";

# let response collect in buffer
			usleep(800000);
# read buffer
			sysread(RESPONSE, $line, 1024, 0);
			close RESPONSE;
   		     close(SCMD);

			$array = unpack('H*', $line);
			my $len = length($array);
# unpacked
			for($i=0; $i < $len; ){
				$result = $result.substr($array, $i , 2).",";
				$i += 2;
			}
			if($len <= 4){
				$result = "NO RESPONSE";   # ignore junk due to hardware
			}
# made string	
			print	"Received from DUT after waiting   : $result \n\n";
			while ((<>) != "\n"){};	# wait for CR from user
		}

