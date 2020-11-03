#include<fstream>
#include<vector>
#include<thread>
#include<iostream>
#include <mutex>
#include <algorithm>
#include <atomic>

using namespace std::chrono_literals;
std::atomic_int count = 0;

using Graph = std::vector<std::vector<std::pair<int, int>>>;

void threadDijkstra(Graph & graph, std::vector<uint64_t> & distance,
	std::vector<bool> & used, int index, int threadCount);

void dijkstra(Graph & graph, std::vector<uint64_t> & distance, int threadCount)
{
	std::vector<bool> used(graph.size(), false);
	distance[0] = 0;
	used[0] = true;
	std::vector<std::thread> threads;
	for(int i = 0; i < threadCount; ++i)
	{
		threads.emplace_back(
			threadDijkstra,
			std::ref(graph),
			std::ref(distance),
			std::ref(used),
			i,
			threadCount);
	}
	for(int i = 0; i < threadCount; ++i)
	{
		if (threads[i].joinable())
			threads[i].join();
	}
	
}

int currentVertex = 0;
std::mutex m, mu;
std::condition_variable cv;

void threadDijkstra(Graph & graph, std::vector<uint64_t> & distance, 
					std::vector<bool> & used, int index, volatile int threadCount)
{
	std::unique_lock<std::mutex> lk(m);
	
	int step1 = graph.size() / threadCount;
	int counter1 = graph.size() % threadCount;
	int first1 = step1 * index + std::min(index, counter1);
	int last1 = first1 + step1;
	if (index < counter1)
		++last1;
	
	for(int i = 1; i <= graph.size(); ++i)
	{
		int step = graph[currentVertex].size() / threadCount;
		int counter = graph[currentVertex].size() % threadCount;
		int first = step * index + std::min(index, counter);
		int last = first + step;
		if (index < counter)
			++last;
		
		for(int j = first; j < last; ++j)
		{
			int v = graph[currentVertex][j].first;
			int weight = graph[currentVertex][j].second;
			if(distance[v] > distance[currentVertex] + weight)
			{
				distance[v] = distance[currentVertex] + weight;
			}
			
		}

		++count;
		if (count >= 2 * i *threadCount - threadCount)
			cv.notify_all();
		else
			cv.wait(lk, [i, threadCount]() {return  count >= 2 * i *threadCount - threadCount; });
		
		int minv = -1;
		for(int j = first1; j < last1; ++j)
		{
			if (!used[j] && (minv == - 1 || distance[j] < distance[minv] ))
				minv = j;
		}
		
		{
			std::lock_guard<std::mutex> lock(mu);
			if (minv != -1 && (distance[minv] < distance[currentVertex] || used[currentVertex]))
				currentVertex = minv;
		}
		++count;
		if (count >= 2 * i *threadCount)
			cv.notify_all();
		else {
			
			cv.wait(lk, [i, threadCount]() {return  count >= 2 * i *threadCount; });
		}
		
		used[currentVertex] = true;
	}
}

int main()
{
	std::ifstream fin("datafile.txt");
	std::ofstream fout("output.txt");
	const int threadCount = 2;
	int m, n;
	fin >> m >> n;
	Graph g(n);
	
	for (int i = 0; i < m; ++i) {
		int x, y, w;
		fin >> x >> y >> w;
		g[x - 1].emplace_back(std::make_pair(y - 1, w));
		g[y - 1].emplace_back(std::make_pair(x - 1, w));
	}

	std::vector<uint64_t> d(g.size(), UINT64_MAX);

	auto start = std::chrono::system_clock::now();

	dijkstra(g, d, threadCount);

	auto end = std::chrono::system_clock::now();
	std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "\n";
	return 0;
}