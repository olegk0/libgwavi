CXXFLAGS =	-O2 -g -Wall -fmessage-length=0

TARGET =	test_jpg

all:	test_jpg test_png

test_jpg:	test_jpg.o GWAVI.o
	$(CXX) -o test_jpg test_jpg.o GWAVI.o

test_png:	test_png.o GWAVI.o
	$(CXX) -o test_png test_png.o GWAVI.o

clean:
	rm -f test_jpg.o test_png.o GWAVI.o test_jpg test_png
