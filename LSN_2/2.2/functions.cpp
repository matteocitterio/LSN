#include "functions.h"

using namespace std;

Cosine::Cosine()
{

    /*
    Cosine empty constructor
    */

    m_a = 1;
    m_b = 1;
    m_c = 0;
}

Cosine::Cosine(double a, double b, double c)
{

    /*
    Cosine constructor
    */

    m_a = a;
    m_b = b;
    m_c = c;
}

Parabola::Parabola()
{

    /*
    Parabola empty constructor
    */

    m_a = 1;
    m_b = 1;
    m_c = 1;
}

Parabola::Parabola(double a, double b, double c)
{

    /*
    Parabola constructor
    */

    m_a = a;
    m_b = b;
    m_c = c;
}