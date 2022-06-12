APP = face_project decompress compress

LDFLAGS = `pkg-config opencv4 --cflags --libs`  -ldlib  -lcudnn -lcuda -lcudart -lcublas -lcurand -lcusolver 
#LDFLAGS := -Wl,-rpath,/usr/local/cuda-11.5/lib64 ../dlib/build/dlib/libdlib.a \
/usr/local/cuda-11.5/lib64/libcudart_static.a -lpthread -ldl -lrt -lpthread -ldl -lrt -lX11 \
 /usr/local/cuda-11.5/lib64/libcublas.so /usr/local/cuda-11.5/lib64/libcudnn.so \
 /usr/local/cuda-11.5/lib64/libcurand.so /usr/local/cuda-11.5/lib64/libcusolver.so \
 /usr/local/cuda-11.5/lib64/libcudart.so -pthread `pkg-config opencv4 --libs` \
 -I ../dlib/ 

CFLAGS := -O1 -g -DNDEBUG -DDLIB_NO_GUI_SUPPORT `pkg-config opencv4 --cflags` #-DDLIB_USE_CUDA  //没有CUDA支持。。


all: $(APP)

.o:%.cpp
	g++ -c $^ -o $@

mmod: mmod.o
	/usr/bin/c++ $(CFLAGS)  $^ -o $@ $(LDFLAGS)

decompress: decompress.o
	/usr/bin/c++ $(CFLAGS)  $^ -o $@ 

compress: dxt.h compress.o
	/usr/bin/c++ $(CFLAGS)  $^ -o $@ 

face_project: face_detection.cpp
#	nvcc  $(CFLAGS) -DDEBUG  -lpthread  -lX11 face_detection.cpp -o $@ `pkg-config opencv4 --cflags --libs` -I ../../../dlib ../../../dlib/dlib/all/source.cpp	
	nvcc  $(CFLAGS) -DDEBUG    -lX11 face_detection.cpp -o $@ `pkg-config opencv4 --cflags --libs`  -ldlib  
	
#	-lcudnn \
	 -lcuda -lcudart -lcublas -lcurand -lcusolver -Xcompiler "-fopenmp"

#不用-lpthread也可以

clean:
	rm $(APP) *.o
#-pg
	 
# -I ../../dlib ../../dlib/dlib/all/source.cpp
	
	
# -I ../dlib ../dlib/dlib/all/source.cpp
