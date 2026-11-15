#pragma once
namespace fault {
// -1 disables injection; zero rejects every allocation; N permits N requests.
void arm(long count);
void disable();
long failures();
}
