#ifndef FINDINFILES_H
#define FINDINFILES_H

#include "config.h"

#include <gtkmm.h>
#include <thread>
#include <mutex>
#include <queue>

class CFindInFiles {
public:
	void SetResultCallback(std::function<bool (std::string)> cb);
	void StartSearch(std::string basepath, std::string text);
private:
	Glib::Dispatcher dispatcher;
	std::thread *search_thread;
	
	// mutex-protected queue for search thread->GUI thread
	std::mutex find_mutex;
	std::queue<std::string> finds;
	
	// abort flag for GUI thread->search thread
	bool abort;
	
	// callback through which results are reported
	std::function<bool (std::string)> result_cb;
	// dispatcher emission
	void on_notify();
	
	// these run in the search thread
	void SearchThread(std::string base, std::string path, std::string text);
	void SearchFile(std::string base, std::string fname, std::string text);
};

#endif