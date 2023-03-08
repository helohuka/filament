#ifndef __RESOURCE_H__
#define __RESOURCE_H__ 1

#include "gamedriver/BaseLibs.h"

class Resource
{
	SINGLE_INSTANCE_FLAG(Resource)

public:

   const utils::Path& getRootPath();

private:

};



#endif // !__RESOURCE_H__
