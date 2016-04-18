#ifndef __FLEVEL_HPP__
#define __FLEVEL_HPP__

#include <memory>

class FLevel
{
public:
	FLevel();
	FLevel(int, int, int);
	~FLevel();
	
	void clear();
	void randomize();
	void loadFromFile(std::string);
	char get(int, int);
	void set(int, int, char);	
	
	int getWidth();
	int getHeight();
	int getTileSize();
	
private:
	int m_width, m_height, m_tileSize;
	char* m_data;
};

#endif