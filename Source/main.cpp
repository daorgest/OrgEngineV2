#pragma once
#include "Application.h"
int main()
{
	Application app;
	if (!app.Init()) return -1;
	app.Run();
	app.Cleanup();
	return 0;
}
