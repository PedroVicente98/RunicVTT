#include "Marker.h"

Marker::Marker()
    : index_buffer(nullptr, NULL), shader(0),texture(NULL), vertex_buffer(nullptr, NULL)
{
}

Marker::Marker(const std::string& path)
    : index_buffer(nullptr, NULL), shader(0),texture(path), vertex_buffer(nullptr, NULL)
{
    
}

Marker::~Marker()
{
}
