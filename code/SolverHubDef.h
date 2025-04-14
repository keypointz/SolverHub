#ifndef SOLVERHUBDEF_H
#define SOLVERHUBDEF_H
// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the SOLVERHUB_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// SOLVERHUB_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef SOLVERHUB_EXPORTS
#define SOLVERHUB_API __declspec(dllexport)
#else
#define SOLVERHUB_API __declspec(dllimport)
#endif

#include "pch.h"


#endif