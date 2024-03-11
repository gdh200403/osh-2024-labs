objects = main.o bubblesort.o

bubble_sort: $(objects)
	g++ -o bubble_sort $(objects)

main.o: main.cpp bubblesort.hpp
	g++ -c main.cpp

bubblesort.o: bubblesort.cpp bubblesort.hpp
	g++ -c bubblesort.cpp

# 清理中间文件
clean:
	rm bubble_sort $(objects)