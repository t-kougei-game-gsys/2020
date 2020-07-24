#include "D3DUtility.h"

unsigned int D3D::Get256Times (unsigned int size) {
	return ((size + 0xff) & ~0xff);
}