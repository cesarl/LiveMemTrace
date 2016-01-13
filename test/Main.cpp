#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

class WorkerThread
{
public:
	WorkerThread()
	{
		for (int i = 0; i < 4; ++i)
		{
			_threads[i] = std::thread([this](){
				while (_exit == false)
				{
					std::function<void()> fn;
					{
						std::unique_lock<std::mutex> lock(_mutex);
						_cond.wait(lock);
						if (_exit)
							return;
						fn = _queue.back();
						_queue.pop();
					}
					fn();
				}
			});
		}
	}

	void push(std::function<void()> fn)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_queue.push(fn);
		_cond.notify_one();
	}

	void exit()
	{
		_exit = true;
		push([](){});
		_cond.notify_all();
		for (int i = 0; i < 4; ++i)
		{
			_threads[i].join();
		}
	}
private:
	std::mutex _mutex;
	std::queue<std::function<void()>> _queue;
	std::condition_variable _cond;
	std::thread _threads[4];
	bool _exit = false;
};

static WorkerThread g_workerThread;

#define LMT_ENABLED 1
#define LMT_ALLOC_NUMBER_PER_CHUNK 1024 * 16
#define LMT_ALLOC_DICTIONARY_SIZE 1024 * 16 * 16
#define LMT_STACK_DICTIONARY_SIZE 1024 * 16 * 8
#define LMT_STACK_SIZE_PER_ALLOC 50
#define LMT_CHUNK_NUMBER_PER_THREAD 4
#define LMT_CACHE_SIZE 16
#define LMT_DEBUG_DEV 1
#define LMT_IMGUI 1
#define LMT_IMGUI_INCLUDE_PATH "External/imgui/imgui.h"
#define LMT_USE_MALLOC ::malloc
#define LMT_USE_REALLOC ::realloc
#define LMT_USE_FREE ::free
#define LMT_DEBUG_DEV 1

#define LMT_STATS 1

#define LMT_IMPL 1
//#define SINGLE_THREADED 1

#if defined(SINGLE_THREADED)
#define LMT_TREAT_CHUNK(chunk) LiveMemTracer::treatChunk(chunk)
#else
#define LMT_TREAT_CHUNK(chunk) g_workerThread.push([=](){LiveMemTracer::treatChunk(chunk);})
#endif

#include "../Src/LiveMemTracer.hpp"

#include <vector>

#include LMT_IMGUI_INCLUDE_PATH
#include "External/GL/gl3w.h"
#include "External/GLFW/glfw3.h"
#include "External/imgui/imgui_impl_glfw_gl3.h"

#include <string>

static void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error %d: %s\n", error, description);
}

#define OVERRIDE_NEW 1

#if defined(OVERRIDE_NEW)
//////////////////////////////////////////////////////////////////////////
void *myMalloc(size_t size)
{
	return LMT_ALLOC(size);
}

void myFree(void *ptr)
{
	LMT_DEALLOC(ptr);
}

void *myRealloc(void *ptr, size_t size)
{
	return LMT_REALLOC(ptr, size);
}

void* operator new(size_t count) throw(std::bad_alloc)
{
	return LMT_ALLOC(count);
}

void* operator new(size_t count, const std::nothrow_t&) throw()
{
	return LMT_ALLOC(count);
}

void* operator new(size_t count, size_t alignment) throw(std::bad_alloc)
{
	return LMT_ALLOC_ALIGNED(count, alignment);
}

void* operator new(size_t count, size_t alignment, const std::nothrow_t&) throw()
{
	return LMT_ALLOC_ALIGNED(count, alignment);
}

void* operator new[](size_t count) throw(std::bad_alloc)
{
	return LMT_ALLOC(count);
}

void* operator new[](size_t count, const std::nothrow_t&) throw()
{
	return LMT_ALLOC(count);
}

void* operator new[](size_t count, size_t alignment) throw(std::bad_alloc)
{
	return LMT_ALLOC_ALIGNED(count, alignment);
}

void* operator new[](size_t count, size_t alignment, const std::nothrow_t&) throw()
{
	return LMT_ALLOC_ALIGNED(count, alignment);
}

void operator delete(void* ptr) throw()
{
	return LMT_DEALLOC(ptr);
}

void operator delete(void *ptr, const std::nothrow_t&) throw()
{
	return LMT_DEALLOC(ptr);
}

void operator delete(void *ptr, size_t alignment) throw()
{
	return LMT_DEALLOC_ALIGNED(ptr);
}

void operator delete(void *ptr, size_t alignment, const std::nothrow_t&) throw()
{
	return LMT_DEALLOC_ALIGNED(ptr);
}

void operator delete[](void* ptr) throw()
{
	return LMT_DEALLOC(ptr);
}

void operator delete[](void *ptr, const std::nothrow_t&) throw()
{
	return LMT_DEALLOC(ptr);
}

void operator delete[](void *ptr, size_t alignment) throw()
{
	return LMT_DEALLOC_ALIGNED(ptr);
}

void operator delete[](void *ptr, size_t alignment, const std::nothrow_t&) throw()
{
	return LMT_DEALLOC_ALIGNED(ptr);
}
//////////////////////////////////////////////////////////////////////////
#else
inline void *myMalloc(size_t size) { return ::malloc(size); }
inline void myFree(void *ptr) { ::free(ptr); }
inline void *myRealloc(void *ptr, size_t size) { return ::realloc(ptr, size); }
#endif

struct Bar
{
	Bar(){}
	char empty[8];
};

struct Foo
{
	size_t       index;
	std::string  indexStr;
	Bar*         ptr;
	Foo*         next;
	Foo() :
		next(nullptr)
	{}
};

template <size_t size>
struct FooBarFunctor
{
	static void call(Foo *prev)
	{
		if (size == 0)
			return;
		Foo *tmpFoo = new Foo;
		tmpFoo->index = size;
		tmpFoo->indexStr = std::to_string(size);
		tmpFoo->ptr = new Bar[size]();
		for (int i = 0; i < size; ++i)
			tmpFoo->indexStr += "|coucou|";
		prev->next = tmpFoo;
		FooBarFunctor<(size > 0 ? size - 1 : 0)>::call(tmpFoo);
	}
};

struct Titi
{
	char titi[256];
};

Titi *leftTiti() { return new Titi; }
Titi *rightTiti() { return new Titi; }

Titi *conditionalTiti(bool left)
{
	if (left)
		return leftTiti();
	else
		return rightTiti();
}

struct Tata
{
	char tata[1024];
	Titi *a;
	Titi *b;
	void *v;
	Tata()
	{
		a = conditionalTiti(true);
		b = conditionalTiti(false);
		v = LMT_ALLOC(128);
	}
	~Tata()
	{
		LMT_DEALLOC(v);
		delete a;
		delete b;
	}
};

struct Toto
{
	char toto[128];
	Tata *one;
	Tata *two;
	Toto()
	{
		one = new Tata();
		two = new Tata();
	}
	~Toto()
	{
		delete one;
		delete two;
	}
};

void smallLeak()
{
	auto t = new Titi();
}

#include <chrono>
#include <iostream>

int main(int ac, char **av)
{
	LMT_INIT();

	//for (int j = 0; j < 500; ++j)
	//{
	//	void *f = nullptr;

	//	size_t size1 = rand() % 1013;
	//	if (size1 == 0) size1 = 1;
	//	auto c = (char*)LMT_ALLOC_ALIGNED(sizeof(char) * size1, 16);
	//	for (int i = 0; i < size1; ++i)
	//	{
	//		c[i] = char(i % 128);
	//	}
	//	for (int a = 0; a < 10; ++a)
	//	{
	//		size_t size2 = rand() % 10133;
	//		if (size2 == 0) size2 = 1;
	//		int* cc = (int*)LMT_REALLOC_ALIGNED(c, sizeof(int) * size2, 16);
	//		for (int i = 0; i < size2; ++i)
	//		{
	//			cc[i] = int(i);
	//		}

	//		struct Prout
	//		{
	//			int a[3];
	//			char b[2];
	//			bool c[31];
	//		};

	//		size_t size3 = rand() % 101331;
	//		if (size3 == 0) size3 = 1;
	//		Prout* ccc = (Prout*)LMT_REALLOC_ALIGNED(cc, sizeof(Prout) * size3, 16);
	//		for (int i = 0; i < size3; ++i)
	//		{
	//			ccc[i] = Prout();
	//			ccc[i].a[2] = i;
	//			ccc[i].b[1] = 'd';
	//			ccc[i].c[30] = false;
	//		}
	//		f = ccc;
	//		c = (char*)ccc;
	//	}
	//	LMT_DEALLOC_ALIGNED(f);
	//}

	// Setup window
	glfwSetErrorCallback(error_callback);
	if (!glfwInit())
		exit(1);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	GLFWwindow* window = glfwCreateWindow(1280, 720, "ImGui OpenGL3 example", NULL, NULL);
	glfwMakeContextCurrent(window);
	gl3wInit();

	// Setup ImGui binding
	ImGui_ImplGlfwGL3_Init(window, true);
	ImGui::GetIO().MemAllocFn = &myMalloc;
	ImGui::GetIO().MemFreeFn = &myFree;

	ImVec4 clear_color = ImColor(114, 144, 154);

	smallLeak();

	// Main loop
	float dt = 0.0f;
	std::vector<Toto*> totoVector;
	int clearCounter = 0;
	while (!glfwWindowShouldClose(window))
	{
		auto start = std::chrono::high_resolution_clock::now();

		glfwPollEvents();
		ImGui_ImplGlfwGL3_NewFrame();

		// Rendering
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);

		Foo foo;
		FooBarFunctor<1>::call(&foo);

		FooBarFunctor<200>::call(foo.next);

		Foo *p = foo.next;
		while (p != nullptr)
		{
			auto *next = p->next;
			delete[]p->ptr;
			delete  p;
			p = next;
		}

		for (int i = 0; i < 100; ++i)
		{
			auto lambda = [&](){
				totoVector.push_back(new Toto());
			};
			lambda();
		}
		if (++clearCounter == 100)
		{
			for (auto &e : totoVector)
			{
				delete e;
			}
			totoVector.clear();
			clearCounter = 0;
		}

		LMT_DISPLAY(dt);
		ImGui::ShowTestWindow();
		ImGui::Render();
		glfwSwapBuffers(window);


		auto end = std::chrono::high_resolution_clock::now();
		int64_t elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		dt = float(elapsedTime) / 1000.f;
	}

	ImGui_ImplGlfwGL3_Shutdown();
	glfwTerminate();
	LMT_EXIT();
	g_workerThread.exit();
	return 0;
}