#!/usr/bin/perl
## -w
#


my $inputfile = "HART.sql";
my $outputfile = "hmt_tablefile";

	open INFILE, "<$inputfile" || die "can't open input: $inputfile \n";

	open OUTFILE, ">$outputfile" || die "can't create output: $outputfle \n";

NEXTLINE:
	while (<INFILE>){
		chomp;
		if(!( $_ =~ /INSERT INTO/)){
			goto NEXTLINE;
		}
		$_ =~ s/INSERT INTO hart_tool VALUES \(//g; # remove string
		$_ =~ s/\);//g;			# remove closing part of line


		($dt,$cn,$rq,$rs,$sz,$pt,$rx,$tt,$ry,$tn) = split(/,/);

		printf OUTFILE "%d,%d,%d,%d,%d,%1s,%d,%16s,%d,%d\n",
			$dt,$cn,$rq,$rs,$sz,$pt,$rx,$tt,$ry,$tn;
	}
	close INFILE;
	close OUTFILE;
