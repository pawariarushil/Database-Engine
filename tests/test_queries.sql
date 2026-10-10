CREATE TABLE students (id INT PRIMARY KEY, name TEXT, gpa FLOAT);

INSERT INTO students VALUES (1, 'Alice', 3.9);
INSERT INTO students VALUES (2, 'Bob', 3.2);
INSERT INTO students VALUES (3, 'Charlie', 3.7);
INSERT INTO students VALUES (4, 'Dana', 2.8);

SELECT * FROM students;

SELECT name, gpa FROM students WHERE gpa >= 3.5;

SELECT * FROM students WHERE id = 3;

UPDATE students SET gpa = 4.0 WHERE id = 1;

SELECT * FROM students WHERE id = 1;

DELETE FROM students WHERE gpa < 3.0;

SELECT * FROM students;

INSERT INTO students VALUES (1, 'Duplicate', 1.0);
