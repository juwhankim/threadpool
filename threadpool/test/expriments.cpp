#include <iostream>
#include "thread.h"

using namespace ::std;

class c
{
public:
	float h(float x) { cout << "I am a member function" << endl; return x; };
	float operator()(int x) { cout << "Hi" << " " << x << endl; return 20.0; };
};

void e() { cout << "Hello" << endl; };

void f(int i, float g) { cout << i << " " << g <<endl; }

int main()
{

	c cc;

	auto g=[&](int x)->float { cout << x << endl; return 20.0; };
	auto h=[] { cout << "Hello" << endl; };
	auto i=[] (int x){ cout << "Hello" << x << endl; };

	juwhan::std::thread th1{f, 1, 2.0};
	juwhan::std::thread th2{h};
	juwhan::std::thread th3{i, 100};
	juwhan::std::thread th4{cc, 100};
	juwhan::std::thread th5{&c::h, &cc, 100.0};

	th1.join();
	th2.join();
	th3.join();
	th4.join();
	th5.join();


	return 0;
}