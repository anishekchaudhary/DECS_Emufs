EMUFS: Emulated File System
Introduction
EMUFS is a lightweight, emulated file system designed for academic and experimental purposes. It supports basic file operations, handles directory structures, and includes optional encryption for secure data storage. This project demonstrates the core functionalities of a file system, including block allocation, inode management, and metadata handling.

Features
  Non-encrypted and Encrypted Modes: Toggle between secure (AES-based encryption) and non-secure file storage.
  Basic File Operations: Create, read, write, delete files, and directories.
  Inode and Block Management: Efficient resource allocation using bitmaps.
  Scalable Design: Supports up to 32 inodes and 64 blocks.
  Logging: Transaction logs for all operations to ensure traceability.
  User-Friendly Interface: Command-driven interface for managing the file system.

Future Scope
  Add journaling for improved fault tolerance.
  Introduce advanced encryption methods.
  Optimize block allocation strategies.
  Expand scalability to support larger devices.
  Implement networked file system capabilities.
Acknowledgments
  This project was created to demonstrate fundamental file system concepts. Special thanks to open-source tools and resources that aided development.

License
  This project is open-source under the MIT License. Feel free to use, modify, and distribute it for educational and experimental purposes.
