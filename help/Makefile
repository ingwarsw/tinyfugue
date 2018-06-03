CC=gcc
CFLAGS=-O2

all: tf-help
	cp tf-help ../lib/tf

tf-help: html2tf commands/*.html topics/*.html
	./html2tf commands/*.html > tf-help || { rm tf-help; exit 1; }
	./html2tf topics/*.html >> tf-help || { rm tf-help; exit 1; }

#debug:
#	@# Test for missing quotes
#	egrep '<a href="[^"]*>' */*.html && false || true

clean:
	rm -f html2tf tf-help


