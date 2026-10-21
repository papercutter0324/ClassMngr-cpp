#pragma once

class WorkspaceStore;

// Transitional source name retained for callers that still depend on the
// DatabaseSession API while persistence ownership moves to WorkspaceStore.
using DatabaseSession = WorkspaceStore;
