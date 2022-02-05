#include "StdAfx.h"
#include "Backend.h"
#include "PCapBackend.h"

NetworkBackend::~NetworkBackend()
{
}

std::shared_ptr<NetworkBackend> NetworkBackend::createBackend()
{
	std::shared_ptr<NetworkBackend> backend(new PCapBackend(PCapBackend::tfe_interface));
	return backend;
}
