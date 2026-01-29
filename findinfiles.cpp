#include "findinfiles.h"
#include "mainwindow.h"

#include <string.h>
#include <giomm/file.h>

void CFindInFiles::SetResultCallback(std::function<bool (std::string)> cb)
{
	result_cb = cb;
}

void CFindInFiles::StartSearch(std::string basepath, std::string text)
{
	dispatcher.connect(sigc::mem_fun(*this, &CFindInFiles::on_notify));
	
	// abort running searches
	if(search_thread) {
		abort=true;
		search_thread->join();
		delete search_thread;
		search_thread=NULL;
	}
	std::queue<std::string>().swap(finds); //clear any pending finds
	
	abort=false;
	search_thread = new std::thread([=,this]{SearchThread(basepath, "", text);});
}

void CFindInFiles::SearchThread(std::string base, std::string path, std::string text)
{
	printf("searchthread(%s,%s,%s)\n",base.c_str(),path.c_str(),text.c_str());
	try {
		Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(base+path);
		Glib::RefPtr<Gio::FileEnumerator> child_enumeration = file->enumerate_children("standard::name,standard::type",Gio::FILE_QUERY_INFO_NONE);
		
		Glib::RefPtr<Gio::FileInfo> file_info;
		std::vector<Glib::RefPtr<Gio::FileInfo> > files;
		while ((file_info = child_enumeration->next_file())) files.push_back(file_info);
		
		for( auto file_info : files )
		{
			// did we receive an abort signal from above?
			if(abort) return;
			
			std::string fname = file_info->get_name(); 
			printf("check %s\n", fname.c_str());
			
			if(fname.length() && fname[0]=='.') continue; // skip dotfiles/-folders

			bool is_dir = file_info->get_file_type()==Gio::FILE_TYPE_DIRECTORY;
			
			if(is_dir) { 
				SearchThread(base, path+"/"+fname, text);
			} else {
				size_t pos;
				if((pos = fname.find(".md"))==fname.length()-3) {
					// search text inside file
					SearchFile(base, path+"/"+fname, text);
				}
			}
		}
	} catch(Gio::Error &e) {
		printf("%s: Gio error: %s\n",__func__, e.what().c_str());
	}
}

void CFindInFiles::SearchFile(std::string base, std::string fname, std::string text)
{
	try {
		char *buf; gsize length;
		
		Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(base + "/" + fname);
		file->load_contents(buf, length);
		
		std::vector<char*> lines;
		
		lines.push_back(buf);
		
		char *ptr = buf, *ptrnext;
		while(ptrnext = strchr(ptr,'\n')) {
			*ptrnext=0;
			++ptrnext;
			lines.push_back(ptrnext);
			ptr=ptrnext;
		}
		
		for(int i=0; i<lines.size(); ++i) {
			char *found_loc;
			if(found_loc = strstr(lines[i], text.c_str())) {
				char buf[512];
				sprintf(buf,"[%s](%s) line %d : char %d\n", fname.c_str(), fname.c_str(), i, found_loc-lines[i]);
				std::string hit = buf;
				/*if(i>0) {
					hit += lines[i-1];
					hit += "\n";
				}*/
				hit += lines[i];
				hit += "\n";
				/*if(i<(lines.size()-1)) {
					hit += lines[i+1];
					hit += "\n";
				}*/
				hit += "---\n";
				{
					std::unique_lock<std::mutex> lock(find_mutex);
					finds.push(hit);
				}
				dispatcher.emit();
				
			}
		}
		
		g_free(buf);
	} catch(Gio::Error &e) {
		printf("%s: Gio error: %s\n",__func__, e.what().c_str());
	}
}

void CFindInFiles::on_notify()
{
	std::unique_lock<std::mutex> lock(find_mutex);
	while(!finds.empty()) {
		if(!abort && !result_cb(finds.front())) {
			abort=true;
		}
		finds.pop();
	}
}