#pragma once
#include "Platform.h"

int main()
{
	Platform::WindowContext wc;
	Platform::Init(&wc);
	Platform::ShowWindow(wc);

	bool running = true;
	while (running)
	{
		running = Platform::ProcessMessages();
	}
}
