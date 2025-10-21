//
// Created by Orgest on 6/9/2025.
//

#pragma once

// for future audio
namespace Audio
{
	struct System
	{
		void* internal;
	};

	bool Init(System* system);
	void Shutdown();
}