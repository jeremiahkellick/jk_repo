#include <cstring>
#include <stdio.h>

constexpr int STARTING_SIZE = 4;

class ExceptionOutOfRange
{
};

class DynamicArray
{
  public:
    DynamicArray()
        : start{0}, end{0}, count{0}, buffer_size{STARTING_SIZE}, buffer{new int[STARTING_SIZE]}
    {
    }

    ~DynamicArray()
    {
        delete[] buffer;
    }

    int &operator[](int i)
    {
        if (!(i >= 0 && i < count)) {
            throw ExceptionOutOfRange{};
        }
        return buffer[(start + i) % buffer_size];
    }

    int size()
    {
        return count;
    }

    void push(int item)
    {
        if (count == buffer_size) {
            expand();
        }
        buffer[end] = item;
        end = (end + 1) % buffer_size;
        count++;
    }

    int pop()
    {
        if (count == 0) {
            throw ExceptionOutOfRange{};
        }
        end = end - 1;
        if (end < 0) {
            end += buffer_size;
        }
        count--;
        return buffer[end];
    }

    void unshift(int item)
    {
        if (count == buffer_size) {
            expand();
        }
        start = start - 1;
        if (start < 0) {
            start += buffer_size;
        }
        buffer[start] = item;
        count++;
    }

    int shift()
    {
        if (count == 0) {
            throw ExceptionOutOfRange{};
        }
        int value = buffer[start];
        start = (start + 1) % buffer_size;
        count--;
        return value;
    }

  private:
    // Index marking the start of the array in the ring buffer, inclusive
    int start;
    // Index marking the end of the array in the ring buffer, exclusive
    int end;
    int count;
    int buffer_size;
    int *buffer;

    void expand()
    {
        int new_buffer_size = buffer_size + (buffer_size / 2);
        int *new_buffer = new int[new_buffer_size];
        if (start < end) {
            std::memcpy(new_buffer, buffer + start, (end - start) * sizeof(int));
        } else {
            int copy1_count = buffer_size - start;
            std::memcpy(new_buffer, buffer + start, copy1_count * sizeof(int));
            std::memcpy(new_buffer + copy1_count, buffer, end * sizeof(int));
        }
        delete[] buffer;
        buffer_size = new_buffer_size;
        buffer = new_buffer;
        start = 0;
        end = count;
    }
};

int main()
{
    int num_squares_to_print = 10;
    DynamicArray array;
    for (int i = 0; i < num_squares_to_print; i++) {
        array.push((i + 2) * (i + 2));
    }
    int first = array.shift();
    int second = array.shift();
    printf("shifted values: %d, %d\n", first, second);
    printf("popped value: %d\n", array.pop());
    array.unshift(999);
    for (int i = 0; i < array.size(); i++) {
        printf("%d%s", array[i], i == (array.size() - 1) ? "\n" : ", ");
    }
    return 0;
}
