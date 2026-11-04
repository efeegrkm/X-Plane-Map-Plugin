// Earcut.cpp
//
// Asagidaki kod mapbox/earcut algoritmasinin yeniden yazimidir.
// Orijinal lisans:
//
//   ISC License
//   Copyright (c) 2016, Mapbox
//
//   Permission to use, copy, modify, and/or distribute this software for any
//   purpose with or without fee is hereby granted, provided that the above
//   copyright notice and this permission notice appear in all copies.
//
//   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
//   WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
//   MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
//   SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
//   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
//   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
//   IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
// Isaret kurali (orijinaldeki gibi): Area(p, q, r) < 0 ise q kosesi disbukey.
// Dis sinir LinkedList(.., true) ile, delikler (.., false) ile kurulur;
// boylece hangi yonde verildikleri fark etmez.
#include "data/Earcut.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <deque>
#include <limits>

namespace
{

struct Node
{
    int mIndex = 0;  // pVertices icindeki indeks
    double mX = 0.0;
    double mY = 0.0;
    Node* mPrev = nullptr;
    Node* mNext = nullptr;
    // Z-duzeni (Morton kodu) siralamasi: kulak testinde sadece ucgenin
    // kutusuna dusen noktalara bakmak icin ikinci bir liste
    int32_t mZ = 0;
    Node* mPrevZ = nullptr;
    Node* mNextZ = nullptr;
    bool mSteiner = false;  // tek noktali delik: filtrelenmez
};

const size_t kHashThreshold = 80;  // bundan az noktada Z-duzeni zahmetine degmez

class Triangulator
{
public:
    explicit Triangulator(const std::vector<LocalPoint>& pVertices)
        : mVertices(pVertices)
    {
    }

    std::vector<int> Run(const std::vector<int>& pOuter, const std::vector<std::vector<int>>& pHoles)
    {
        Node* tOuter = LinkedList(pOuter, true);
        if (!tOuter || tOuter->mNext == tOuter->mPrev)
        {
            return mTriangles;
        }
        if (!pHoles.empty())
        {
            tOuter = EliminateHoles(pHoles, tOuter);
        }
        if (mVertices.size() > kHashThreshold)
        {
            double tMaxX = mVertices[0].mX;
            double tMaxY = mVertices[0].mY;
            mMinX = tMaxX;
            mMinY = tMaxY;
            for (const LocalPoint& tPoint : mVertices)
            {
                mMinX = std::min(mMinX, tPoint.mX);
                mMinY = std::min(mMinY, tPoint.mY);
                tMaxX = std::max(tMaxX, tPoint.mX);
                tMaxY = std::max(tMaxY, tPoint.mY);
            }
            const double tSize = std::max(tMaxX - mMinX, tMaxY - mMinY);
            mInvSize = tSize != 0.0 ? 32767.0 / tSize : 0.0;
        }
        EarcutLinked(tOuter, 0);
        return mTriangles;
    }

private:
    // --- Kucuk geometri yardimcilari ---

    static double Area(const Node* pP, const Node* pQ, const Node* pR)
    {
        return (pQ->mY - pP->mY) * (pR->mX - pQ->mX) - (pQ->mX - pP->mX) * (pR->mY - pQ->mY);
    }

    static bool Equals(const Node* pA, const Node* pB)
    {
        return pA->mX == pB->mX && pA->mY == pB->mY;
    }

    static int Sign(double pValue)
    {
        return (pValue > 0.0) - (pValue < 0.0);
    }

    static bool PointInTriangle(double pAx, double pAy, double pBx, double pBy, double pCx, double pCy, double pPx,
                                double pPy)
    {
        return (pCx - pPx) * (pAy - pPy) >= (pAx - pPx) * (pCy - pPy) &&
               (pAx - pPx) * (pBy - pPy) >= (pBx - pPx) * (pAy - pPy) &&
               (pBx - pPx) * (pCy - pPy) >= (pCx - pPx) * (pBy - pPy);
    }

    // q, p-r dogru parcasinin ustunde mi (uc noktalar dogrusal iken)
    static bool OnSegment(const Node* pP, const Node* pQ, const Node* pR)
    {
        return pQ->mX <= std::max(pP->mX, pR->mX) && pQ->mX >= std::min(pP->mX, pR->mX) &&
               pQ->mY <= std::max(pP->mY, pR->mY) && pQ->mY >= std::min(pP->mY, pR->mY);
    }

    static bool Intersects(const Node* pP1, const Node* pQ1, const Node* pP2, const Node* pQ2)
    {
        const int tO1 = Sign(Area(pP1, pQ1, pP2));
        const int tO2 = Sign(Area(pP1, pQ1, pQ2));
        const int tO3 = Sign(Area(pP2, pQ2, pP1));
        const int tO4 = Sign(Area(pP2, pQ2, pQ1));
        if (tO1 != tO2 && tO3 != tO4)
        {
            return true;
        }
        return (tO1 == 0 && OnSegment(pP1, pP2, pQ1)) || (tO2 == 0 && OnSegment(pP1, pQ2, pQ1)) ||
               (tO3 == 0 && OnSegment(pP2, pP1, pQ2)) || (tO4 == 0 && OnSegment(pP2, pQ1, pQ2));
    }

    // a-b kosegeni poligonun baska bir kenarini kesiyor mu
    static bool IntersectsPolygon(const Node* pA, const Node* pB)
    {
        const Node* tP = pA;
        do
        {
            if (tP->mIndex != pA->mIndex && tP->mNext->mIndex != pA->mIndex && tP->mIndex != pB->mIndex &&
                tP->mNext->mIndex != pB->mIndex && Intersects(tP, tP->mNext, pA, pB))
            {
                return true;
            }
            tP = tP->mNext;
        } while (tP != pA);
        return false;
    }

    // a'dan b'ye giden kosegen, a kosesinde poligonun icine mi bakiyor
    static bool LocallyInside(const Node* pA, const Node* pB)
    {
        return Area(pA->mPrev, pA, pA->mNext) < 0.0
                   ? Area(pA, pB, pA->mNext) >= 0.0 && Area(pA, pA->mPrev, pB) >= 0.0
                   : Area(pA, pB, pA->mPrev) < 0.0 || Area(pA, pA->mNext, pB) < 0.0;
    }

    // a-b kosegeninin orta noktasi poligonun icinde mi (isin atma)
    static bool MiddleInside(const Node* pA, const Node* pB)
    {
        const Node* tP = pA;
        bool tInside = false;
        const double tPx = (pA->mX + pB->mX) * 0.5;
        const double tPy = (pA->mY + pB->mY) * 0.5;
        do
        {
            if (((tP->mY > tPy) != (tP->mNext->mY > tPy)) && tP->mNext->mY != tP->mY &&
                (tPx < (tP->mNext->mX - tP->mX) * (tPy - tP->mY) / (tP->mNext->mY - tP->mY) + tP->mX))
            {
                tInside = !tInside;
            }
            tP = tP->mNext;
        } while (tP != pA);
        return tInside;
    }

    // Orijinaldeki kosullarin aynisi; sadece sirasi farkli: once O(1) testler,
    // en son butun kenarlari tarayan IntersectsPolygon (O(n)). Bolme adiminda
    // her kose cifti denendigi icin bu sira buyuk poligonlarda cok fark eder
    // (WAHI: 3.1 s).
    static bool IsValidDiagonal(const Node* pA, const Node* pB)
    {
        if (pA->mNext->mIndex == pB->mIndex || pA->mPrev->mIndex == pB->mIndex)
        {
            return false;
        }
        const bool tVisible = LocallyInside(pA, pB) && LocallyInside(pB, pA) &&
                              (Area(pA->mPrev, pA, pB->mPrev) != 0.0 || Area(pA, pB->mPrev, pB) != 0.0);
        const bool tZeroLength =
            Equals(pA, pB) && Area(pA->mPrev, pA, pA->mNext) > 0.0 && Area(pB->mPrev, pB, pB->mNext) > 0.0;
        if (!tVisible && !tZeroLength)
        {
            return false;
        }
        if (IntersectsPolygon(pA, pB))
        {
            return false;
        }
        return tZeroLength || MiddleInside(pA, pB);
    }

    static bool SectorContainsSector(const Node* pM, const Node* pP)
    {
        return Area(pM->mPrev, pM, pP->mPrev) < 0.0 && Area(pP->mNext, pM, pM->mNext) < 0.0;
    }

    // --- Cift yonlu halka listesi ---

    Node* InsertNode(int pIndex, Node* pLast)
    {
        mPool.emplace_back();
        Node* tNode = &mPool.back();
        tNode->mIndex = pIndex;
        tNode->mX = mVertices[pIndex].mX;
        tNode->mY = mVertices[pIndex].mY;
        if (!pLast)
        {
            tNode->mPrev = tNode;
            tNode->mNext = tNode;
        }
        else
        {
            tNode->mNext = pLast->mNext;
            tNode->mPrev = pLast;
            pLast->mNext->mPrev = tNode;
            pLast->mNext = tNode;
        }
        return tNode;
    }

    static void RemoveNode(Node* pNode)
    {
        pNode->mNext->mPrev = pNode->mPrev;
        pNode->mPrev->mNext = pNode->mNext;
        if (pNode->mPrevZ)
        {
            pNode->mPrevZ->mNextZ = pNode->mNextZ;
        }
        if (pNode->mNextZ)
        {
            pNode->mNextZ->mPrevZ = pNode->mPrevZ;
        }
    }

    Node* NewNode(const Node* pFrom)
    {
        mPool.emplace_back();
        Node* tNode = &mPool.back();
        tNode->mIndex = pFrom->mIndex;
        tNode->mX = pFrom->mX;
        tNode->mY = pFrom->mY;
        return tNode;
    }

    double SignedArea(const std::vector<int>& pRing) const
    {
        double tSum = 0.0;
        for (size_t tIndex = 0, tPrevious = pRing.size() - 1; tIndex < pRing.size(); tPrevious = tIndex++)
        {
            const LocalPoint& tA = mVertices[pRing[tPrevious]];
            const LocalPoint& tB = mVertices[pRing[tIndex]];
            tSum += (tA.mX - tB.mX) * (tB.mY + tA.mY);
        }
        return tSum;
    }

    // Halkayi istenen yonde listeye cevirir
    Node* LinkedList(const std::vector<int>& pRing, bool pClockwise)
    {
        if (pRing.empty())
        {
            return nullptr;
        }
        Node* tLast = nullptr;
        if (pClockwise == (SignedArea(pRing) > 0.0))
        {
            for (int tIndex : pRing)
            {
                tLast = InsertNode(tIndex, tLast);
            }
        }
        else
        {
            for (auto tIterator = pRing.rbegin(); tIterator != pRing.rend(); ++tIterator)
            {
                tLast = InsertNode(*tIterator, tLast);
            }
        }
        if (tLast && Equals(tLast, tLast->mNext))
        {
            Node* tNext = tLast->mNext;
            RemoveNode(tLast);
            tLast = tNext;
        }
        return tLast;
    }

    // Tekrar eden ve dogrusal noktalari atar
    static Node* FilterPoints(Node* pStart, Node* pEnd = nullptr)
    {
        if (!pStart)
        {
            return pStart;
        }
        if (!pEnd)
        {
            pEnd = pStart;
        }
        Node* tP = pStart;
        bool tAgain = false;
        do
        {
            tAgain = false;
            if (!tP->mSteiner && (Equals(tP, tP->mNext) || Area(tP->mPrev, tP, tP->mNext) == 0.0))
            {
                RemoveNode(tP);
                tP = pEnd = tP->mPrev;
                if (tP == tP->mNext)
                {
                    break;
                }
                tAgain = true;
            }
            else
            {
                tP = tP->mNext;
            }
        } while (tAgain || tP != pEnd);
        return pEnd;
    }

    // a ile b'yi kosegenle bolup iki halka yapar; ikinci halkanin bir dugumunu doner
    Node* SplitPolygon(Node* pA, Node* pB)
    {
        Node* tA2 = NewNode(pA);
        Node* tB2 = NewNode(pB);
        Node* tAn = pA->mNext;
        Node* tBp = pB->mPrev;
        pA->mNext = pB;
        pB->mPrev = pA;
        tA2->mNext = tAn;
        tAn->mPrev = tA2;
        tB2->mNext = tA2;
        tA2->mPrev = tB2;
        tBp->mNext = tB2;
        tB2->mPrev = tBp;
        return tB2;
    }

    // --- Ucgenleme ---

    bool IsEar(const Node* pEar) const
    {
        const Node* tA = pEar->mPrev;
        const Node* tB = pEar;
        const Node* tC = pEar->mNext;
        if (Area(tA, tB, tC) >= 0.0)
        {
            return false;  // icbukey: kulak olamaz
        }
        const double tMinX = std::min({tA->mX, tB->mX, tC->mX});
        const double tMaxX = std::max({tA->mX, tB->mX, tC->mX});
        const double tMinY = std::min({tA->mY, tB->mY, tC->mY});
        const double tMaxY = std::max({tA->mY, tB->mY, tC->mY});
        // Ucgenin icinde icbukey bir kose varsa kulak degil
        for (const Node* tP = tC->mNext; tP != tA; tP = tP->mNext)
        {
            if (tP->mX >= tMinX && tP->mX <= tMaxX && tP->mY >= tMinY && tP->mY <= tMaxY &&
                PointInTriangle(tA->mX, tA->mY, tB->mX, tB->mY, tC->mX, tC->mY, tP->mX, tP->mY) &&
                Area(tP->mPrev, tP, tP->mNext) >= 0.0)
            {
                return false;
            }
        }
        return true;
    }

    // Noktanin Z-duzeni kodu: x ve y'nin bitleri sirayla dizilir. Yakin
    // noktalarin kodlari da yakin olur.
    int32_t ZOrder(double pX, double pY) const
    {
        uint32_t tX = static_cast<uint32_t>((pX - mMinX) * mInvSize);
        uint32_t tY = static_cast<uint32_t>((pY - mMinY) * mInvSize);
        tX = (tX | (tX << 8)) & 0x00FF00FFu;
        tX = (tX | (tX << 4)) & 0x0F0F0F0Fu;
        tX = (tX | (tX << 2)) & 0x33333333u;
        tX = (tX | (tX << 1)) & 0x55555555u;
        tY = (tY | (tY << 8)) & 0x00FF00FFu;
        tY = (tY | (tY << 4)) & 0x0F0F0F0Fu;
        tY = (tY | (tY << 2)) & 0x33333333u;
        tY = (tY | (tY << 1)) & 0x55555555u;
        return static_cast<int32_t>(tX | (tY << 1));
    }

    // Halkanin dugumlerini Z-duzenine gore ikinci bir listeye dizer
    void IndexCurve(Node* pStart) const
    {
        Node* tP = pStart;
        do
        {
            if (tP->mZ == 0)
            {
                tP->mZ = ZOrder(tP->mX, tP->mY);
            }
            tP->mPrevZ = tP->mPrev;
            tP->mNextZ = tP->mNext;
            tP = tP->mNext;
        } while (tP != pStart);
        tP->mPrevZ->mNextZ = nullptr;
        tP->mPrevZ = nullptr;
        SortLinked(tP);
    }

    // Bagli liste birlestirme siralamasi (Simon Tatham yontemi), mZ'ye gore
    static Node* SortLinked(Node* pList)
    {
        int tInSize = 1;
        int tMerges = 0;
        do
        {
            Node* tP = pList;
            pList = nullptr;
            Node* tTail = nullptr;
            tMerges = 0;
            while (tP)
            {
                ++tMerges;
                Node* tQ = tP;
                int tPSize = 0;
                for (int tStep = 0; tStep < tInSize; ++tStep)
                {
                    ++tPSize;
                    tQ = tQ->mNextZ;
                    if (!tQ)
                    {
                        break;
                    }
                }
                int tQSize = tInSize;
                while (tPSize > 0 || (tQSize > 0 && tQ))
                {
                    Node* tE = nullptr;
                    if (tPSize != 0 && (tQSize == 0 || !tQ || tP->mZ <= tQ->mZ))
                    {
                        tE = tP;
                        tP = tP->mNextZ;
                        --tPSize;
                    }
                    else
                    {
                        tE = tQ;
                        tQ = tQ->mNextZ;
                        --tQSize;
                    }
                    if (tTail)
                    {
                        tTail->mNextZ = tE;
                    }
                    else
                    {
                        pList = tE;
                    }
                    tE->mPrevZ = tTail;
                    tTail = tE;
                }
                tP = tQ;
            }
            tTail->mNextZ = nullptr;
            tInSize *= 2;
        } while (tMerges > 1);
        return pList;
    }

    static bool BlocksEar(const Node* pP, const Node* pA, const Node* pB, const Node* pC, double pMinX, double pMaxX,
                          double pMinY, double pMaxY)
    {
        return pP->mX >= pMinX && pP->mX <= pMaxX && pP->mY >= pMinY && pP->mY <= pMaxY && pP != pA && pP != pC &&
               PointInTriangle(pA->mX, pA->mY, pB->mX, pB->mY, pC->mX, pC->mY, pP->mX, pP->mY) &&
               Area(pP->mPrev, pP, pP->mNext) >= 0.0;
    }

    // IsEar'in hizli hali: sadece Z kodu ucgenin kutusunun Z araliginda olan noktalara bakar
    bool IsEarHashed(const Node* pEar) const
    {
        const Node* tA = pEar->mPrev;
        const Node* tB = pEar;
        const Node* tC = pEar->mNext;
        if (Area(tA, tB, tC) >= 0.0)
        {
            return false;
        }
        const double tMinX = std::min({tA->mX, tB->mX, tC->mX});
        const double tMaxX = std::max({tA->mX, tB->mX, tC->mX});
        const double tMinY = std::min({tA->mY, tB->mY, tC->mY});
        const double tMaxY = std::max({tA->mY, tB->mY, tC->mY});
        const int32_t tMinZ = ZOrder(tMinX, tMinY);
        const int32_t tMaxZ = ZOrder(tMaxX, tMaxY);
        const Node* tP = pEar->mPrevZ;
        const Node* tN = pEar->mNextZ;
        while (tP && tP->mZ >= tMinZ && tN && tN->mZ <= tMaxZ)
        {
            if (BlocksEar(tP, tA, tB, tC, tMinX, tMaxX, tMinY, tMaxY))
            {
                return false;
            }
            tP = tP->mPrevZ;
            if (BlocksEar(tN, tA, tB, tC, tMinX, tMaxX, tMinY, tMaxY))
            {
                return false;
            }
            tN = tN->mNextZ;
        }
        while (tP && tP->mZ >= tMinZ)
        {
            if (BlocksEar(tP, tA, tB, tC, tMinX, tMaxX, tMinY, tMaxY))
            {
                return false;
            }
            tP = tP->mPrevZ;
        }
        while (tN && tN->mZ <= tMaxZ)
        {
            if (BlocksEar(tN, tA, tB, tC, tMinX, tMaxX, tMinY, tMaxY))
            {
                return false;
            }
            tN = tN->mNextZ;
        }
        return true;
    }

    void Emit(const Node* pA, const Node* pB, const Node* pC)
    {
        mTriangles.push_back(pA->mIndex);
        mTriangles.push_back(pB->mIndex);
        mTriangles.push_back(pC->mIndex);
    }

    // Kucuk yerel kesismeleri (a-p ile p.next-b kesisiyorsa) ucgen ekleyerek giderir
    Node* CureLocalIntersections(Node* pStart)
    {
        Node* tP = pStart;
        do
        {
            Node* tA = tP->mPrev;
            Node* tB = tP->mNext->mNext;
            if (!Equals(tA, tB) && Intersects(tA, tP, tP->mNext, tB) && LocallyInside(tA, tB) &&
                LocallyInside(tB, tA))
            {
                Emit(tA, tP, tB);
                RemoveNode(tP);
                RemoveNode(tP->mNext);
                tP = pStart = tB;
            }
            tP = tP->mNext;
        } while (tP != pStart);
        return FilterPoints(tP);
    }

    // Son care: gecerli bir kosegen bul, poligonu ikiye bol, ikisini ayri ucgenle
    void SplitEarcut(Node* pStart)
    {
        Node* tA = pStart;
        do
        {
            Node* tB = tA->mNext->mNext;
            while (tB != tA->mPrev)
            {
                if (tA->mIndex != tB->mIndex && IsValidDiagonal(tA, tB))
                {
                    Node* tC = SplitPolygon(tA, tB);
                    tA = FilterPoints(tA, tA->mNext);
                    tC = FilterPoints(tC, tC->mNext);
                    EarcutLinked(tA, 0);
                    EarcutLinked(tC, 0);
                    return;
                }
                tB = tB->mNext;
            }
            tA = tA->mNext;
        } while (tA != pStart);
    }

    // pPass: 0 normal, 1 noktalar filtrelendi, 2 yerel kesismeler giderildi
    void EarcutLinked(Node* pEar, int pPass)
    {
        if (!pEar)
        {
            return;
        }
        const bool tHashed = mInvSize != 0.0;
        if (pPass == 0 && tHashed)
        {
            IndexCurve(pEar);
        }
        Node* tStop = pEar;
        while (pEar->mPrev != pEar->mNext)
        {
            Node* tPrevious = pEar->mPrev;
            Node* tNext = pEar->mNext;
            if (tHashed ? IsEarHashed(pEar) : IsEar(pEar))
            {
                Emit(tPrevious, pEar, tNext);
                RemoveNode(pEar);
                // Bir sonraki koseyi atlamak ince (serit) ucgenleri azaltir
                pEar = tNext->mNext;
                tStop = tNext->mNext;
                continue;
            }
            pEar = tNext;
            if (pEar == tStop)
            {
                // Tum halka dolasildi, kulak bulunamadi: sirayla daha guclu yontemler
                if (pPass == 0)
                {
                    EarcutLinked(FilterPoints(pEar), 1);
                }
                else if (pPass == 1)
                {
                    EarcutLinked(CureLocalIntersections(FilterPoints(pEar)), 2);
                }
                else
                {
                    SplitEarcut(pEar);
                }
                break;
            }
        }
    }

    // --- Delikler ---

    static Node* Leftmost(Node* pStart)
    {
        Node* tP = pStart;
        Node* tLeftmost = pStart;
        do
        {
            if (tP->mX < tLeftmost->mX || (tP->mX == tLeftmost->mX && tP->mY < tLeftmost->mY))
            {
                tLeftmost = tP;
            }
            tP = tP->mNext;
        } while (tP != pStart);
        return tLeftmost;
    }

    // Deligin en soldaki noktasindan sola isin: dis sinirda baglanacak koseyi bul
    static Node* FindHoleBridge(const Node* pHole, Node* pOuter)
    {
        Node* tP = pOuter;
        const double tHx = pHole->mX;
        const double tHy = pHole->mY;
        double tQx = -std::numeric_limits<double>::infinity();
        Node* tM = nullptr;
        do
        {
            if (tHy <= tP->mY && tHy >= tP->mNext->mY && tP->mNext->mY != tP->mY)
            {
                const double tX = tP->mX + (tHy - tP->mY) * (tP->mNext->mX - tP->mX) / (tP->mNext->mY - tP->mY);
                if (tX <= tHx && tX > tQx)
                {
                    tQx = tX;
                    tM = tP->mX < tP->mNext->mX ? tP : tP->mNext;
                    if (tX == tHx)
                    {
                        return tM;  // delik dis kenara degiyor
                    }
                }
            }
            tP = tP->mNext;
        } while (tP != pOuter);
        if (!tM)
        {
            return nullptr;
        }

        // Isin ile bulunan nokta arasindaki ucgende baska kose varsa, isinla en
        // kucuk aciyi yapani sec (o kose deligi "goruyor")
        const Node* tStop = tM;
        const double tMx = tM->mX;
        const double tMy = tM->mY;
        double tTanMin = std::numeric_limits<double>::infinity();
        tP = tM;
        do
        {
            if (tHx >= tP->mX && tP->mX >= tMx && tHx != tP->mX &&
                PointInTriangle(tHy < tMy ? tHx : tQx, tHy, tMx, tMy, tHy < tMy ? tQx : tHx, tHy, tP->mX, tP->mY))
            {
                const double tTan = std::fabs(tHy - tP->mY) / (tHx - tP->mX);
                if (LocallyInside(tP, pHole) &&
                    (tTan < tTanMin ||
                     (tTan == tTanMin && (tP->mX > tM->mX || (tP->mX == tM->mX && SectorContainsSector(tM, tP))))))
                {
                    tM = tP;
                    tTanMin = tTan;
                }
            }
            tP = tP->mNext;
        } while (tP != tStop);
        return tM;
    }

    Node* EliminateHole(Node* pHole, Node* pOuter)
    {
        Node* tBridge = FindHoleBridge(pHole, pOuter);
        if (!tBridge)
        {
            return pOuter;  // delik dis sinirin disinda: yok say
        }
        Node* tBridgeReverse = SplitPolygon(tBridge, pHole);
        FilterPoints(tBridgeReverse, tBridgeReverse->mNext);
        return FilterPoints(tBridge, tBridge->mNext);
    }

    Node* EliminateHoles(const std::vector<std::vector<int>>& pHoles, Node* pOuter)
    {
        std::vector<Node*> tQueue;
        for (const std::vector<int>& tHole : pHoles)
        {
            Node* tList = LinkedList(tHole, false);
            if (!tList)
            {
                continue;
            }
            if (tList == tList->mNext)
            {
                tList->mSteiner = true;
            }
            tQueue.push_back(Leftmost(tList));
        }
        std::sort(tQueue.begin(), tQueue.end(), [](const Node* pA, const Node* pB)
                  {
                      return pA->mX < pB->mX;
                  });
        for (Node* tHole : tQueue)
        {
            pOuter = EliminateHole(tHole, pOuter);
        }
        return pOuter;
    }

    const std::vector<LocalPoint>& mVertices;
    double mMinX = 0.0;
    double mMinY = 0.0;
    double mInvSize = 0.0;  // 0: Z-duzeni kullanilmiyor (az nokta)
    std::deque<Node> mPool;  // deque: ekleme yapinca eski dugumlerin adresi degismez
    std::vector<int> mTriangles;
};

}  // namespace

namespace Earcut
{

std::vector<int> Triangulate(const std::vector<LocalPoint>& pVertices, const std::vector<int>& pOuter,
                             const std::vector<std::vector<int>>& pHoles)
{
    Triangulator tTriangulator(pVertices);
    return tTriangulator.Run(pOuter, pHoles);
}

}  // namespace Earcut
