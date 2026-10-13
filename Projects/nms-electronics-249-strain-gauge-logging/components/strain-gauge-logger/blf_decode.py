from can import BLFReader
import traceback


def main():
    reader = BLFReader("strain_gauge_log_007.blf")
    count = 0
    addr = 144 + 32

    try:
        for msg in reader:
            print(msg)
            count += 1
            addr += 48
    except Exception:
        traceback.print_exc()
        print("Failure parsing n=" + str(count) + " at addr: " + hex(addr))


if __name__ == '__main__':
    main()
