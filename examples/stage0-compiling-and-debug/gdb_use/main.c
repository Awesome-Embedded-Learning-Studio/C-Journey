int main()
{
	int a = 0;
	int i = 0;
	int arr[3] = {0, 1, 2};
	for(i = 0; i < 3; i++)
		arr[i] += 1;
	int* p = arr;
	p[0] = p[1];
	return 0;
}
